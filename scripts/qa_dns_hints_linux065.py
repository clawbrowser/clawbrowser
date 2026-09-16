"""Observe browser hint controls and managed cases, without changing DNS."""
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time

root = Path('/opt/clawbrowser-qa')
label = os.environ.get('QA_DNS_HINT_LABEL', 'dns-hints-065')
assert re.fullmatch(r'dns-hints-065(?:-(?:protected|normalized)-15s)?', label)
seconds = int(os.environ.get('CLAWBROWSER_DNS_HINT_SECONDS', '4'))
assert 4 <= seconds <= 30
private = root / 'secrets' / label
private.mkdir(mode=0o700)
report = root / 'evidence' / (label + '.json')
assert not report.exists()
capture_path = private / 'dns.txt'
capture_error = private / 'tcpdump.txt'
with capture_path.open('x') as output, capture_error.open('x') as errors:
    capture = subprocess.Popen(['tcpdump', '-i', 'any', '-nn', '-l', '-vv', 'port 53'],
                               stdout=output, stderr=errors)
    try:
        for _ in range(50):
            if 'listening on' in capture_error.read_text():
                break
            assert capture.poll() is None
            time.sleep(.1)
        else:
            raise RuntimeError('Capture did not start')
        tests = subprocess.run([
            'runuser', '-u', 'builder', '--', 'env', '-u', 'DBUS_SESSION_BUS_ADDRESS',
            'CLAWBROWSER_TEST_HEADFUL=1', 'CLAWBROWSER_DNS_HINT_CAPTURE=1',
            'CLAWBROWSER_BINARY=' + str(root / 'release-extract-cached-ua-065/clawbrowser-linux-x64/clawbrowser'),
            'xvfb-run', '-a', str(root / 'integration-venv/bin/python'), '-m', 'pytest',
            '-q', '-o', 'junit_family=xunit1',
            'clawbrowser/test/integration/test_proxy_dns_hints.py',
            '--junitxml=' + str(root / 'evidence' / (label + '.xml'))],
            cwd=root / 'test-repo-d68176f', timeout=180)
        time.sleep(1)
    finally:
        capture.send_signal(signal.SIGINT)
        capture.wait(timeout=10)
lines = capture_path.read_text().splitlines()
counts = {kind: {hint: sum(f'qa-hint-{kind}-{hint}-' in line for line in lines)
                 for hint in ('dns', 'connect')}
          for kind in ('direct', 'http', 'socks5', 'socks5-auth')}
drops = re.search(r'(\d+) packets dropped by kernel', capture_error.read_text())
result = {'artifact': 'cached-ua-065', 'test_exit_code': tests.returncode,
          'dns_port53_observations': counts,
          'capture_drops': int(drops[1]) if drops else None,
          'observation_seconds_per_case': seconds,
          'canvas_mode': os.environ.get('CLAWBROWSER_QA_NORMALIZED_CANVAS', '0'),
          'scope': 'HTTP document hints; not DoH/DoT or HTTPS documents'}
result['accepted'] = (tests.returncode == 0 and result['capture_drops'] == 0
    and all(counts['direct'].values())
    and all(value == 0 for kind in ('http', 'socks5', 'socks5-auth') for value in counts[kind].values()))
with report.open('x') as output:
    json.dump(result, output, indent=2)
print(json.dumps(result))
assert result['accepted'], 'Hint gate failed or positive controls absent'

