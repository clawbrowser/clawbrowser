import socket
import struct
import threading

import pytest

from stun_control import assert_stun_server_responds


@pytest.mark.parametrize("response", ["valid", "wrong_transaction", "wrong_cookie", "truncated", "request"])
def test_stun_control_validates_response(response):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as server:
        server.bind(("127.0.0.1", 0))
        server.settimeout(2)
        port = server.getsockname()[1]

        def serve():
            packet, peer = server.recvfrom(2048)
            result = struct.pack("!HHI", 0x0101, 0, 0x2112A442) + packet[8:20]
            if response == "wrong_transaction":
                result = result[:8] + bytes(b ^ 1 for b in result[8:])
            elif response == "wrong_cookie":
                result = result[:4] + b"bad!" + result[8:]
            elif response == "truncated":
                result = result[:10]
            elif response == "request":
                result = packet
            server.sendto(result, peer)

        thread = threading.Thread(target=serve)
        thread.start()
        try:
            if response == "valid":
                assert_stun_server_responds(f"stun:127.0.0.1:{port}", timeout=0.2)
            else:
                with pytest.raises(AssertionError, match="inconclusive"):
                    assert_stun_server_responds(f"stun:127.0.0.1:{port}", timeout=0.2)
        finally:
            thread.join(timeout=2)
        assert not thread.is_alive()


def test_stun_control_rejects_unresponsive_endpoint():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as silent:
        silent.bind(("127.0.0.1", 0))
        with pytest.raises(AssertionError, match="inconclusive"):
            assert_stun_server_responds(f"stun:127.0.0.1:{silent.getsockname()[1]}", timeout=0.05)


@pytest.mark.parametrize("url", ["https://127.0.0.1", "stun:", "stun:127.0.0.1:0",
    "stun:127.0.0.1:99999", "stun:user:private@127.0.0.1", "stun:127.0.0.1/private"])
def test_stun_control_rejects_invalid_endpoint_without_echoing_it(url):
    with pytest.raises(AssertionError) as error:
        assert_stun_server_responds(url)
    assert "private" not in str(error.value)
