"""Independent UDP STUN liveness control; never run inside the browser."""

import secrets
import socket
import struct
from urllib.parse import urlsplit


def assert_stun_server_responds(url, timeout=3):
    if not isinstance(url, str) or not url.startswith("stun:"):
        raise AssertionError("Control requires a stun: UDP URL")
    try:
        parsed = urlsplit("//" + url[5:])
        host = parsed.hostname
        port = 3478 if parsed.port is None else parsed.port
        if (not host or not 1 <= port <= 65535 or parsed.username
                or parsed.password or parsed.path or parsed.query or parsed.fragment):
            raise ValueError()
        addresses = socket.getaddrinfo(host, port, type=socket.SOCK_DGRAM)
    except (ValueError, OSError):
        raise AssertionError("Invalid or unresolvable STUN control endpoint") from None
    transaction = secrets.token_bytes(12)
    cookie = 0x2112A442
    request = struct.pack("!HHI", 1, 0, cookie) + transaction
    for family, kind, proto, _, address in addresses:
        try:
            with socket.socket(family, kind, proto) as control:
                control.settimeout(timeout)
                control.connect(address)
                control.send(request)
                reply = control.recv(2048)
                if len(reply) < 20:
                    continue
                message_type, length, received_cookie = struct.unpack("!HHI", reply[:8])
                if (message_type == 0x0101 and received_cookie == cookie
                        and reply[8:20] == transaction and length % 4 == 0
                        and len(reply) == 20 + length):
                    return
        except OSError:
            continue
    raise AssertionError("Configured STUN server did not answer the independent control; leak test is inconclusive")
