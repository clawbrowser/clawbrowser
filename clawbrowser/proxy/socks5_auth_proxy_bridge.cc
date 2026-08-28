#include "clawbrowser/proxy/socks5_auth_proxy_bridge.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace clawbrowser {

namespace {

#if BUILDFLAG(IS_WIN)
using SocketFd = SOCKET;
using SocketFlags = u_long;
using SocketLength = int;
using SocketPollFd = WSAPOLLFD;
using SocketByteCount = int;
constexpr SocketFd kInvalidSocket = INVALID_SOCKET;
constexpr int kShutdownBoth = SD_BOTH;
#else
using SocketFd = int;
using SocketFlags = int;
using SocketLength = socklen_t;
using SocketPollFd = pollfd;
using SocketByteCount = ssize_t;
constexpr SocketFd kInvalidSocket = -1;
constexpr int kShutdownBoth = SHUT_RDWR;
#endif

constexpr size_t kMaxHttpHeaderBytes = 64 * 1024;
constexpr size_t kRelayBufferSize = 16 * 1024;
constexpr int kConnectTimeoutSeconds = 15;
constexpr int kSocketIoTimeoutSeconds = 30;
constexpr int kRelayIdleTimeoutSeconds = 300;
constexpr int kMaxActiveConnections = 128;

struct BridgeConfig {
  std::string upstream_host;
  int upstream_port = 0;
  std::string username;
  std::string password;
};

struct TargetEndpoint {
  std::string host;
  int port = 0;
};

struct ParsedHttpRequest {
  bool is_connect = false;
  TargetEndpoint target;
  std::string bytes_to_forward;
  std::string buffered_tunnel_bytes;
};

#if BUILDFLAG(IS_WIN)
bool EnsureSocketApiInitialized() {
  static const bool initialized = [] {
    WSADATA wsa_data = {};
    return WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0;
  }();
  return initialized;
}
#else
bool EnsureSocketApiInitialized() {
  return true;
}
#endif

int GetSocketLastError() {
#if BUILDFLAG(IS_WIN)
  return WSAGetLastError();
#else
  return errno;
#endif
}

bool IsConnectInProgressError(int error) {
#if BUILDFLAG(IS_WIN)
  return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS;
#else
  return error == EINPROGRESS;
#endif
}

bool IsAcceptClosedError(int error) {
#if BUILDFLAG(IS_WIN)
  return error == WSAENOTSOCK || error == WSAEINVAL;
#else
  return error == EBADF || error == EINVAL;
#endif
}

void CloseSocket(SocketFd fd) {
#if BUILDFLAG(IS_WIN)
  closesocket(fd);
#else
  close(fd);
#endif
}

SocketByteCount SendSocket(SocketFd fd, const char* data, size_t length) {
#if BUILDFLAG(IS_WIN)
  const size_t chunk =
      std::min(length, static_cast<size_t>(std::numeric_limits<int>::max()));
  return send(fd, data, static_cast<int>(chunk), 0);
#else
  return send(fd, data, length, 0);
#endif
}

SocketByteCount RecvSocket(SocketFd fd, char* data, size_t length) {
#if BUILDFLAG(IS_WIN)
  const size_t chunk =
      std::min(length, static_cast<size_t>(std::numeric_limits<int>::max()));
  return recv(fd, data, static_cast<int>(chunk), 0);
#else
  return recv(fd, data, length, 0);
#endif
}

int PollSockets(SocketPollFd* fds, size_t count, int timeout_ms) {
#if BUILDFLAG(IS_WIN)
  return WSAPoll(fds, static_cast<ULONG>(count), timeout_ms);
#else
  return poll(fds, static_cast<nfds_t>(count), timeout_ms);
#endif
}

int SetSocketOption(SocketFd fd,
                    int level,
                    int option_name,
                    const void* option_value,
                    SocketLength option_length) {
#if BUILDFLAG(IS_WIN)
  return setsockopt(fd, level, option_name,
                    reinterpret_cast<const char*>(option_value), option_length);
#else
  return setsockopt(fd, level, option_name, option_value, option_length);
#endif
}

bool GetSocketError(SocketFd fd, int* socket_error) {
  SocketLength socket_error_length = sizeof(*socket_error);
#if BUILDFLAG(IS_WIN)
  return getsockopt(fd, SOL_SOCKET, SO_ERROR,
                    reinterpret_cast<char*>(socket_error),
                    &socket_error_length) == 0;
#else
  return getsockopt(fd, SOL_SOCKET, SO_ERROR, socket_error,
                    &socket_error_length) == 0;
#endif
}

class ActiveConnectionGuard {
 public:
  explicit ActiveConnectionGuard(std::shared_ptr<std::atomic<int>> count)
      : count_(std::move(count)) {}
  ActiveConnectionGuard(const ActiveConnectionGuard&) = delete;
  ActiveConnectionGuard& operator=(const ActiveConnectionGuard&) = delete;
  ~ActiveConnectionGuard() {
    if (count_) {
      count_->fetch_sub(1);
    }
  }

 private:
  std::shared_ptr<std::atomic<int>> count_;
};

class ScopedSocket {
 public:
  explicit ScopedSocket(SocketFd fd = kInvalidSocket) : fd_(fd) {}
  ScopedSocket(const ScopedSocket&) = delete;
  ScopedSocket& operator=(const ScopedSocket&) = delete;
  ScopedSocket(ScopedSocket&& other) noexcept : fd_(other.release()) {}
  ScopedSocket& operator=(ScopedSocket&& other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }
  ~ScopedSocket() { reset(); }

  SocketFd get() const { return fd_; }
  bool is_valid() const { return fd_ != kInvalidSocket; }

  SocketFd release() {
    SocketFd fd = fd_;
    fd_ = kInvalidSocket;
    return fd;
  }

  void reset(SocketFd fd = kInvalidSocket) {
    if (fd_ != kInvalidSocket) {
      CloseSocket(fd_);
    }
    fd_ = fd;
  }

 private:
  SocketFd fd_ = kInvalidSocket;
};

bool HasCompleteSocks5BridgeConfig(const RuntimeProxyConfig& proxy) {
  return proxy.host.has_value() && !proxy.host->empty() &&
         proxy.port.has_value() && proxy.username.has_value() &&
         !proxy.username->empty() && proxy.password.has_value() &&
         !proxy.password->empty();
}

std::optional<TargetEndpoint> ParseAuthority(std::string_view authority,
                                             int default_port) {
  TargetEndpoint target;
  if (authority.empty()) {
    return std::nullopt;
  }

  if (authority.front() == '[') {
    size_t close = authority.find(']');
    if (close == std::string_view::npos) {
      return std::nullopt;
    }
    target.host = std::string(authority.substr(1, close - 1));
    if (close + 1 == authority.size()) {
      target.port = default_port;
      return target;
    }
    if (authority[close + 1] != ':') {
      return std::nullopt;
    }
    int port = 0;
    if (!base::StringToInt(std::string(authority.substr(close + 2)), &port) ||
        port <= 0 || port > 65535) {
      return std::nullopt;
    }
    target.port = port;
    return target;
  }

  size_t separator = authority.rfind(':');
  if (separator == std::string_view::npos) {
    target.host = std::string(authority);
    target.port = default_port;
    return target;
  }

  target.host = std::string(authority.substr(0, separator));
  int port = 0;
  if (target.host.empty() ||
      !base::StringToInt(std::string(authority.substr(separator + 1)),
                         &port) ||
      port <= 0 || port > 65535) {
    return std::nullopt;
  }
  target.port = port;
  return target;
}

std::optional<std::string> HeaderValue(std::string_view headers,
                                       std::string_view name) {
  size_t offset = 0;
  const std::string lower_name = base::ToLowerASCII(std::string(name));
  while (offset < headers.size()) {
    size_t line_end = headers.find("\r\n", offset);
    if (line_end == std::string_view::npos) {
      break;
    }
    std::string_view line = headers.substr(offset, line_end - offset);
    offset = line_end + 2;
    size_t separator = line.find(':');
    if (separator == std::string_view::npos) {
      continue;
    }
    std::string key =
        base::ToLowerASCII(std::string(line.substr(0, separator)));
    if (key != lower_name) {
      continue;
    }
    std::string value(line.substr(separator + 1));
    base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);
    return value;
  }
  return std::nullopt;
}

bool ReadHttpRequest(SocketFd client_fd, std::string* request) {
  request->clear();
  char buffer[4096];
  while (request->size() < kMaxHttpHeaderBytes) {
    SocketByteCount received = RecvSocket(client_fd, buffer, sizeof(buffer));
    if (received <= 0) {
      return false;
    }
    request->append(buffer, static_cast<size_t>(received));
    if (request->find("\r\n\r\n") != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::optional<ParsedHttpRequest> ParseHttpRequest(
    const std::string& request_bytes) {
  size_t header_end = request_bytes.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return std::nullopt;
  }
  size_t first_line_end = request_bytes.find("\r\n");
  if (first_line_end == std::string::npos || first_line_end > header_end) {
    return std::nullopt;
  }

  std::string first_line = request_bytes.substr(0, first_line_end);
  std::istringstream line_stream(first_line);
  std::string method;
  std::string target;
  std::string version;
  line_stream >> method >> target >> version;
  if (method.empty() || target.empty() || version.empty()) {
    return std::nullopt;
  }

  ParsedHttpRequest parsed;
  std::string_view headers =
      std::string_view(request_bytes)
          .substr(first_line_end + 2, header_end - first_line_end - 2);
  const std::string remainder = request_bytes.substr(header_end + 4);

  if (base::EqualsCaseInsensitiveASCII(method, "CONNECT")) {
    std::optional<TargetEndpoint> target_endpoint =
        ParseAuthority(target, 443);
    if (!target_endpoint.has_value()) {
      return std::nullopt;
    }
    parsed.is_connect = true;
    parsed.target = std::move(*target_endpoint);
    parsed.buffered_tunnel_bytes = remainder;
    return parsed;
  }

  constexpr std::string_view kHttpPrefix = "http://";
  if (base::StartsWith(target, kHttpPrefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    std::string_view absolute_target(target);
    std::string_view without_scheme =
        absolute_target.substr(kHttpPrefix.size());
    size_t path_start = without_scheme.find('/');
    std::string_view authority =
        path_start == std::string_view::npos
            ? without_scheme
            : without_scheme.substr(0, path_start);
    std::string path =
        path_start == std::string_view::npos
            ? "/"
            : std::string(without_scheme.substr(path_start));
    std::optional<TargetEndpoint> target_endpoint =
        ParseAuthority(authority, 80);
    if (!target_endpoint.has_value()) {
      return std::nullopt;
    }
    parsed.target = std::move(*target_endpoint);
    parsed.bytes_to_forward =
        method + " " + path + " " + version + "\r\n" +
        request_bytes.substr(first_line_end + 2);
    return parsed;
  }

  std::optional<std::string> host = HeaderValue(headers, "host");
  if (!host.has_value()) {
    return std::nullopt;
  }
  std::optional<TargetEndpoint> target_endpoint = ParseAuthority(*host, 80);
  if (!target_endpoint.has_value()) {
    return std::nullopt;
  }
  parsed.target = std::move(*target_endpoint);
  parsed.bytes_to_forward = request_bytes;
  return parsed;
}

bool SendAll(SocketFd fd, const char* data, size_t length) {
  size_t sent = 0;
  while (sent < length) {
    SocketByteCount result =
        SendSocket(fd, UNSAFE_BUFFERS(data + sent), length - sent);
    if (result <= 0) {
      return false;
    }
    sent += static_cast<size_t>(result);
  }
  return true;
}

bool SendAll(SocketFd fd, std::string_view data) {
  return SendAll(fd, data.data(), data.size());
}

bool SendAll(SocketFd fd, const std::vector<uint8_t>& data) {
  return SendAll(fd, reinterpret_cast<const char*>(data.data()), data.size());
}

bool RecvExact(SocketFd fd, uint8_t* data, size_t length) {
  size_t received_total = 0;
  while (received_total < length) {
    SocketByteCount received = RecvSocket(
        fd, UNSAFE_BUFFERS(reinterpret_cast<char*>(data + received_total)),
        length - received_total);
    if (received <= 0) {
      return false;
    }
    received_total += static_cast<size_t>(received);
  }
  return true;
}

bool RecvExact(SocketFd fd, std::vector<uint8_t>* data, size_t length) {
  data->resize(length);
  return RecvExact(fd, data->data(), length);
}

bool SetSocketTimeouts(SocketFd fd, int seconds) {
#if BUILDFLAG(IS_WIN)
  DWORD timeout_ms = static_cast<DWORD>(seconds * 1000);
  if (SetSocketOption(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout_ms,
                      sizeof(timeout_ms)) != 0) {
    return false;
  }
  if (SetSocketOption(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout_ms,
                      sizeof(timeout_ms)) != 0) {
    return false;
  }
#else
  timeval timeout = {};
  timeout.tv_sec = seconds;
  if (SetSocketOption(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                      sizeof(timeout)) != 0) {
    return false;
  }
  if (SetSocketOption(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                      sizeof(timeout)) != 0) {
    return false;
  }
#endif
  return true;
}

bool SetNonBlocking(SocketFd fd, SocketFlags* original_flags) {
#if BUILDFLAG(IS_WIN)
  *original_flags = 0;
  u_long nonblocking = 1;
  return ioctlsocket(fd, FIONBIO, &nonblocking) == 0;
#else
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return false;
  }
  *original_flags = flags;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

void RestoreSocketFlags(SocketFd fd, SocketFlags flags) {
#if BUILDFLAG(IS_WIN)
  u_long blocking = flags;
  ioctlsocket(fd, FIONBIO, &blocking);
#else
  fcntl(fd, F_SETFL, flags);
#endif
}

bool WaitForConnect(SocketFd fd, int timeout_seconds) {
  SocketPollFd socket_poll = {};
  socket_poll.fd = fd;
  socket_poll.events = POLLOUT;
  int ready = PollSockets(&socket_poll, 1, timeout_seconds * 1000);
  if (ready <= 0 ||
      !(socket_poll.revents & (POLLOUT | POLLERR | POLLHUP))) {
    return false;
  }

  int socket_error = 0;
  if (!GetSocketError(fd, &socket_error)) {
    return false;
  }
  return socket_error == 0;
}

std::optional<ScopedSocket> ConnectTcp(const std::string& host, int port) {
  addrinfo hints = {};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  addrinfo* results = nullptr;
  const std::string port_text = base::NumberToString(port);
  if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &results) != 0) {
    return std::nullopt;
  }
  std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> scoped_results(
      results, &freeaddrinfo);

  for (addrinfo* ai = scoped_results.get(); ai; ai = ai->ai_next) {
    ScopedSocket socket_fd(socket(ai->ai_family, ai->ai_socktype,
                                  ai->ai_protocol));
    if (!socket_fd.is_valid()) {
      continue;
    }

    SocketFlags original_flags = 0;
    if (!SetNonBlocking(socket_fd.get(), &original_flags)) {
      continue;
    }

    int connect_result =
        connect(socket_fd.get(), ai->ai_addr, ai->ai_addrlen);
    if (connect_result == 0 ||
        (connect_result != 0 && IsConnectInProgressError(GetSocketLastError()) &&
         WaitForConnect(socket_fd.get(), kConnectTimeoutSeconds))) {
      RestoreSocketFlags(socket_fd.get(), original_flags);
      SetSocketTimeouts(socket_fd.get(), kSocketIoTimeoutSeconds);
      return std::move(socket_fd);
    }
    RestoreSocketFlags(socket_fd.get(), original_flags);
  }
  return std::nullopt;
}

bool AppendSocksAddress(const std::string& host, std::vector<uint8_t>* request) {
  in_addr ipv4 = {};
  if (inet_pton(AF_INET, host.c_str(), &ipv4) == 1) {
    request->push_back(0x01);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&ipv4);
    request->insert(request->end(), bytes, UNSAFE_BUFFERS(bytes + 4));
    return true;
  }

  in6_addr ipv6 = {};
  if (inet_pton(AF_INET6, host.c_str(), &ipv6) == 1) {
    request->push_back(0x04);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&ipv6);
    request->insert(request->end(), bytes, UNSAFE_BUFFERS(bytes + 16));
    return true;
  }

  if (host.empty() || host.size() > 255) {
    return false;
  }
  request->push_back(0x03);
  request->push_back(static_cast<uint8_t>(host.size()));
  request->insert(request->end(), host.begin(), host.end());
  return true;
}

bool ReadSocks5ConnectReply(SocketFd fd) {
  uint8_t header[4] = {};
  if (!RecvExact(fd, header, sizeof(header)) || header[0] != 0x05 ||
      header[1] != 0x00) {
    return false;
  }

  size_t address_length = 0;
  if (header[3] == 0x01) {
    address_length = 4;
  } else if (header[3] == 0x04) {
    address_length = 16;
  } else if (header[3] == 0x03) {
    uint8_t domain_length = 0;
    if (!RecvExact(fd, &domain_length, 1)) {
      return false;
    }
    address_length = domain_length;
  } else {
    return false;
  }

  std::vector<uint8_t> ignored;
  return RecvExact(fd, &ignored, address_length + 2);
}

bool AuthenticateAndConnectSocks5(SocketFd upstream_fd,
                                  const BridgeConfig& config,
                                  const TargetEndpoint& target) {
  if (config.username.size() > 255 || config.password.size() > 255 ||
      target.port <= 0 || target.port > 65535) {
    return false;
  }

  const uint8_t greeting[] = {0x05, 0x01, 0x02};
  if (!SendAll(upstream_fd, reinterpret_cast<const char*>(greeting),
               sizeof(greeting))) {
    return false;
  }

  uint8_t greeting_response[2] = {};
  if (!RecvExact(upstream_fd, greeting_response, sizeof(greeting_response)) ||
      greeting_response[0] != 0x05 || greeting_response[1] != 0x02) {
    return false;
  }

  std::vector<uint8_t> auth_request;
  auth_request.reserve(3 + config.username.size() + config.password.size());
  auth_request.push_back(0x01);
  auth_request.push_back(static_cast<uint8_t>(config.username.size()));
  auth_request.insert(auth_request.end(), config.username.begin(),
                      config.username.end());
  auth_request.push_back(static_cast<uint8_t>(config.password.size()));
  auth_request.insert(auth_request.end(), config.password.begin(),
                      config.password.end());
  if (!SendAll(upstream_fd, auth_request)) {
    return false;
  }

  uint8_t auth_response[2] = {};
  if (!RecvExact(upstream_fd, auth_response, sizeof(auth_response)) ||
      auth_response[0] != 0x01 || auth_response[1] != 0x00) {
    return false;
  }

  std::vector<uint8_t> connect_request = {0x05, 0x01, 0x00};
  if (!AppendSocksAddress(target.host, &connect_request)) {
    return false;
  }
  connect_request.push_back(static_cast<uint8_t>((target.port >> 8) & 0xff));
  connect_request.push_back(static_cast<uint8_t>(target.port & 0xff));
  if (!SendAll(upstream_fd, connect_request)) {
    return false;
  }

  return ReadSocks5ConnectReply(upstream_fd);
}

void SendHttpError(SocketFd client_fd, std::string_view status) {
  const std::string response =
      "HTTP/1.1 " + std::string(status) +
      "\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
  SendAll(client_fd, response);
}

void RelayBidirectional(SocketFd client_fd, SocketFd upstream_fd) {
  std::vector<char> buffer(kRelayBufferSize);
  while (true) {
    std::array<SocketPollFd, 2> poll_fds = {};
    poll_fds[0].fd = client_fd;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = upstream_fd;
    poll_fds[1].events = POLLIN;

    int ready = PollSockets(poll_fds.data(), poll_fds.size(),
                            kRelayIdleTimeoutSeconds * 1000);
    if (ready <= 0) {
      return;
    }

    if (poll_fds[0].revents & POLLIN) {
      SocketByteCount received =
          RecvSocket(client_fd, buffer.data(), buffer.size());
      if (received <= 0 ||
          !SendAll(upstream_fd, buffer.data(),
                   static_cast<size_t>(received))) {
        return;
      }
    } else if (poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
      return;
    }

    if (poll_fds[1].revents & POLLIN) {
      SocketByteCount received =
          RecvSocket(upstream_fd, buffer.data(), buffer.size());
      if (received <= 0 ||
          !SendAll(client_fd, buffer.data(),
                   static_cast<size_t>(received))) {
        return;
      }
    } else if (poll_fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
      return;
    }
  }
}

bool IsLoopbackPeer(const sockaddr_in& client_address) {
  return client_address.sin_family == AF_INET &&
         ntohl(client_address.sin_addr.s_addr) == INADDR_LOOPBACK;
}

void HandleClient(BridgeConfig config,
                  std::shared_ptr<std::atomic<int>> active_connections,
                  SocketFd accepted_fd) {
  ActiveConnectionGuard active_guard(std::move(active_connections));
  ScopedSocket client_fd(accepted_fd);
  SetSocketTimeouts(client_fd.get(), kSocketIoTimeoutSeconds);

  std::string request_bytes;
  if (!ReadHttpRequest(client_fd.get(), &request_bytes)) {
    return;
  }

  std::optional<ParsedHttpRequest> request = ParseHttpRequest(request_bytes);
  if (!request.has_value()) {
    SendHttpError(client_fd.get(), "400 Bad Request");
    return;
  }

  std::optional<ScopedSocket> upstream =
      ConnectTcp(config.upstream_host, config.upstream_port);
  if (!upstream.has_value() ||
      !AuthenticateAndConnectSocks5(upstream->get(), config, request->target)) {
    SendHttpError(client_fd.get(), "502 Bad Gateway");
    return;
  }

  if (request->is_connect) {
    if (!SendAll(client_fd.get(),
                 "HTTP/1.1 200 Connection Established\r\n\r\n")) {
      return;
    }
    if (!request->buffered_tunnel_bytes.empty() &&
        !SendAll(upstream->get(), request->buffered_tunnel_bytes)) {
      return;
    }
  } else if (!SendAll(upstream->get(), request->bytes_to_forward)) {
    return;
  }

  RelayBidirectional(client_fd.get(), upstream->get());
}

}  // namespace

class Socks5AuthProxyBridge::Impl {
 public:
  explicit Impl(BridgeConfig config) : config_(std::move(config)) {}
  ~Impl() { Stop(); }

  base::expected<void, std::string> Start() {
    if (!EnsureSocketApiInitialized()) {
      return base::unexpected("failed to initialize socket API");
    }

    ScopedSocket listener(socket(AF_INET, SOCK_STREAM, 0));
    if (!listener.is_valid()) {
      return base::unexpected("failed to create SOCKS5 bridge listener");
    }

    int reuse = 1;
    SetSocketOption(listener.get(), SOL_SOCKET, SO_REUSEADDR, &reuse,
                    sizeof(reuse));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(0);
    if (bind(listener.get(), reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) != 0) {
      return base::unexpected("failed to bind SOCKS5 bridge listener");
    }
    if (listen(listener.get(), SOMAXCONN) != 0) {
      return base::unexpected("failed to listen on SOCKS5 bridge listener");
    }

    SocketLength address_length = sizeof(address);
    if (getsockname(listener.get(), reinterpret_cast<sockaddr*>(&address),
                    &address_length) != 0) {
      return base::unexpected("failed to inspect SOCKS5 bridge listener");
    }

    endpoint_ = {.host = "127.0.0.1", .port = ntohs(address.sin_port)};
    listen_fd_ = listener.release();
    running_.store(true);
    accept_thread_ = std::thread(&Impl::AcceptLoop, this);
    return base::ok();
  }

  void Stop() {
    bool was_running = running_.exchange(false);
    if (listen_fd_ != kInvalidSocket) {
      shutdown(listen_fd_, kShutdownBoth);
      CloseSocket(listen_fd_);
      listen_fd_ = kInvalidSocket;
    }
    if (was_running && accept_thread_.joinable()) {
      accept_thread_.join();
    } else if (accept_thread_.joinable()) {
      accept_thread_.join();
    }
  }

  ProxyBridgeEndpoint endpoint() const { return endpoint_; }

 private:
  void AcceptLoop() {
    while (running_.load()) {
      sockaddr_in client_address = {};
      SocketLength client_address_length = sizeof(client_address);
      SocketFd accepted =
          accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address),
                 &client_address_length);
      if (accepted == kInvalidSocket) {
        if (!running_.load() || IsAcceptClosedError(GetSocketLastError())) {
          return;
        }
        continue;
      }

      if (!IsLoopbackPeer(client_address)) {
        CloseSocket(accepted);
        continue;
      }

      int previous_count = active_connections_->fetch_add(1);
      if (previous_count >= kMaxActiveConnections) {
        active_connections_->fetch_sub(1);
        ScopedSocket rejected_fd(accepted);
        SendHttpError(rejected_fd.get(), "503 Service Unavailable");
        continue;
      }

      std::thread(HandleClient, config_, active_connections_, accepted)
          .detach();
    }
  }

  BridgeConfig config_;
  ProxyBridgeEndpoint endpoint_;
  SocketFd listen_fd_ = kInvalidSocket;
  std::atomic<bool> running_{false};
  std::shared_ptr<std::atomic<int>> active_connections_ =
      std::make_shared<std::atomic<int>>(0);
  std::thread accept_thread_;
};

Socks5AuthProxyBridge::Socks5AuthProxyBridge(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

Socks5AuthProxyBridge::~Socks5AuthProxyBridge() = default;

ProxyBridgeEndpoint Socks5AuthProxyBridge::endpoint() const {
  return impl_->endpoint();
}

base::expected<std::unique_ptr<Socks5AuthProxyBridge>, std::string>
Socks5AuthProxyBridge::Start(const RuntimeProxyConfig& proxy) {
  std::string scheme = proxy.scheme.value_or("http");
  base::TrimWhitespaceASCII(scheme, base::TRIM_ALL, &scheme);
  scheme = base::ToLowerASCII(scheme);
  if (scheme != "socks5") {
    return base::unexpected("SOCKS5 auth bridge requires socks5 scheme");
  }

  if (!HasCompleteSocks5BridgeConfig(proxy)) {
    return base::unexpected("SOCKS5 auth bridge requires endpoint credentials");
  }

  BridgeConfig config;
  config.upstream_host = *proxy.host;
  config.upstream_port = *proxy.port;
  config.username = *proxy.username;
  config.password = *proxy.password;

  auto impl = std::make_unique<Impl>(std::move(config));
  auto start_result = impl->Start();
  if (!start_result.has_value()) {
    return base::unexpected(start_result.error());
  }

  return base::ok(std::unique_ptr<Socks5AuthProxyBridge>(
      new Socks5AuthProxyBridge(std::move(impl))));
}

}  // namespace clawbrowser
