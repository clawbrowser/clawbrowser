"""Parse opt-in proxy credentials without depending on developer-only flags."""

from urllib.parse import unquote, urlsplit


def profile_proxy_from_url(value: str) -> dict:
    try:
        parsed = urlsplit(value)
        port = parsed.port
        valid = (
            parsed.scheme in {"http", "socks5"}
            and parsed.hostname
            and port is not None
            and 0 < port <= 65535
            and parsed.path in {"", "/"}
            and not parsed.query
            and not parsed.fragment
        )
        if not valid:
            raise ValueError()
    except ValueError:
        # Never include the input: these URLs contain real credentials.
        raise ValueError("Expected an HTTP or SOCKS5 proxy URL with host and port") from None
    return {
        "scheme": parsed.scheme,
        "host": parsed.hostname,
        "port": port,
        "username": unquote(parsed.username or ""),
        "password": unquote(parsed.password or ""),
        "country": "US",
        "connection_type": "residential",
    }
