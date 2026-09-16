"""Read-only port-53 observation; no resolver or network configuration changes."""
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time
import uuid
import xml.etree.ElementTree as ET

root = Path('/opt/clawbrowser-qa')
label = os.environ.get('QA_TLS_DNS_LABEL', 'tls-dns-065-protected-example')
assert label in ('tls-dns-065-protected-example', 'tls-dns-065-normalized-example')
private = root / 'secrets' / label
private.mkdir(mode=0o700)
report = root / 'evidence' / (label + '.json')
assert not report.exists()
capture_path = private / 'dns.txt'
capture_error = private / 'tcpdump.txt'
with capture_path.open('x') as output, capture_error.open('x') as errors:
    capture = subprocess.Popen(
        ['tcpdump', '-i', 'any', '-nn', '-l', '-vv', 'port 53'],
        stdout=output, stderr=errors)
    try:
        for _ in range(50):
            if 'listening on' in capture_error.read_text():
                break
            assert capture.poll() is None, 'Capture failed to start'
            time.sleep(.1)
        else:
            raise RuntimeError('Capture did not become ready')
        control = 'qa-dns-control-' + uuid.uuid4().hex + '.example.com'
        dig = subprocess.run(['dig', '+tries=1', '+time=3', control, 'A'],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        tests = subprocess.run([
            'runuser', '-u', 'builder', '--', 'env', '-u', 'DBUS_SESSION_BUS_ADDRESS',
            'CLAWBROWSER_TEST_HEADFUL=1',
            'CLAWBROWSER_BINARY=' + str(root / 'release-extract-cached-ua-065/clawbrowser-linux-x64/clawbrowser'),
            'xvfb-run', '-a', str(root / 'integration-venv/bin/python'),
            '-m', 'pytest', '-q', '-o', 'junit_family=xunit1',
            'clawbrowser/test/integration/test_proxy_tls_hostname.py',
            '--junitxml=' + str(root / 'evidence' / (label + '.xml'))],
            cwd=root / 'test-repo-d68176f', timeout=180)
        time.sleep(1)
    finally:
        capture.send_signal(signal.SIGINT)
        capture.wait(timeout=10)

text = capture_path.read_text()
stats = capture_error.read_text()
positive = sum(control in line for line in text.splitlines())
origin = sum('qa-tls-route-' in line for line in text.splitlines())
drops = re.search(r'(\d+) packets dropped by kernel', stats)
cases = ET.parse(root / 'evidence' / (label + '.xml')).findall('.//testcase')
complete = len(cases) == 6 and all(
    case.find('skipped') is None and case.find('failure') is None
    and case.find('error') is None for case in cases)
result = {
    'artifact': 'cached-ua-065', 'headful': True, 'test_exit_code': tests.returncode,
    'control_dig_exit_code': dig.returncode, 'control_dns_observations': positive,
    'origin_dns_observations_port53': origin,
    'capture_drops': int(drops[1]) if drops else None,
    'six_cases_executed_without_skips': complete,
    'canvas_mode': os.environ.get('CLAWBROWSER_QA_NORMALIZED_CANVAS', '0'),
    'scope': 'HTTPS via CONNECT, SOCKS5, authenticated SOCKS5; port 53 only. '
             'Browser uses a temporary SPKI exception; not public CA acceptance, '
             'DoH, DoT, proxy-host DNS, or arbitrary-page prefetch acceptance.'}
with report.open('x') as output:
    json.dump(result, output, indent=2)
print(json.dumps(result))
assert tests.returncode == 0 and dig.returncode == 0
assert complete, 'All six TLS cases must execute; skips are not acceptance'
assert positive > 0 and origin == 0 and result['capture_drops'] == 0
