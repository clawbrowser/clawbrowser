import pytest
import json

from conftest import FIXTURE_DIR, _seed_profile
from real_proxy_config import profile_proxy_from_url


def test_profile_proxy_is_seeded_in_normal_profile_without_changing_fixture(tmp_path):
    fixture = FIXTURE_DIR / "valid_fingerprint.json"
    original = fixture.read_bytes()
    proxy = profile_proxy_from_url("socks5://test:password@proxy.example:1080")
    profile = _seed_profile(tmp_path, "valid_fingerprint.json", proxy_config=proxy)
    saved = json.loads((tmp_path / "Browser/test_profile/fingerprint.json").read_text())
    assert saved["response"]["proxy"] == proxy
    assert profile["response"]["proxy"] == proxy
    assert fixture.read_bytes() == original


def test_profile_proxy_decodes_credentials():
    proxy = profile_proxy_from_url("socks5://test%40user:p%3Ass@proxy.example:1080")
    assert proxy["scheme"] == "socks5"
    assert proxy["username"] == "test@user"
    assert proxy["password"] == "p:ss"


def test_profile_proxy_supports_ipv6_and_no_auth():
    proxy = profile_proxy_from_url("http://[2001:db8::1]:8080")
    assert proxy["host"] == "2001:db8::1"
    assert proxy["password"] == ""


@pytest.mark.parametrize("value", [
    "https://user:secret@proxy.example:443",
    "socks5://user:secret@proxy.example",
    "socks5://user:secret@proxy.example:invalid",
    "socks5://user:secret@proxy.example:0",
    "socks5://user:secret@proxy.example:1080/private",
])
def test_profile_proxy_rejects_invalid_urls_without_disclosing_credentials(value):
    with pytest.raises(ValueError) as error:
        profile_proxy_from_url(value)
    assert "secret" not in str(error.value)
