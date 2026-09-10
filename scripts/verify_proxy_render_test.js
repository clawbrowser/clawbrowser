#!/usr/bin/env node

const assert = require('node:assert/strict');

global.window = {};

const {
  exhaustProxyRetries,
  iceCandidateRelatedAddress,
  iceCandidateType,
  isUnspecifiedIceAddress,
  nextProxyRetryAction,
  sdpIceCandidates,
  shouldRetryProxyResult,
  shouldValidateSurface,
  summarizeWebRtcCandidates,
  surfaceSkipCheck,
  summarizeProxyResult,
} = require('../clawbrowser/verify/resources/verify.js');

assert.deepEqual(window.__clawbrowser_capabilities, {
  managed_proxy_privacy: 2,
});
assert.equal(Object.isFrozen(window.__clawbrowser_capabilities), true);

assert.equal(iceCandidateType({ type: 'relay' }), 'relay');
assert.equal(
  iceCandidateType('candidate:1 1 UDP 1 192.0.2.1 12345 typ srflx'),
  'srflx'
);
assert.equal(
  iceCandidateRelatedAddress(
    'candidate:1 1 UDP 1 203.0.113.5 12345 typ relay raddr 192.0.2.8 rport 9'
  ),
  '192.0.2.8'
);
assert.equal(isUnspecifiedIceAddress('0.0.0.0'), true);
assert.equal(isUnspecifiedIceAddress('::'), true);
assert.equal(isUnspecifiedIceAddress('0.0.0.0:9'), true);
assert.equal(isUnspecifiedIceAddress('[::]:9'), true);
assert.equal(isUnspecifiedIceAddress('192.0.2.8'), false);
assert.deepEqual(sdpIceCandidates([
  'v=0',
  'a=candidate:1 1 UDP 1 192.0.2.1 12345 typ host',
  '',
].join('\r\n')), [{
  candidate: 'candidate:1 1 UDP 1 192.0.2.1 12345 typ host',
  source: 'sdp',
}]);

assert.deepEqual(summarizeWebRtcCandidates({
  candidates: [],
  errors: [],
  complete: true,
}), {
  surface: 'webrtc.iceCandidates',
  pass: true,
  expected: 'completed gathering; relay candidates only; no related or ICE error address',
  actual: 'gathering complete; no ICE candidates exposed',
  detail: 'no direct or related address exposed',
});

assert.equal(summarizeWebRtcCandidates({
  candidates: [
    { candidate: 'candidate:1 1 UDP 1 192.0.2.1 12345 typ relay' },
  ],
  complete: true,
}).pass, true);

const unsafeWebRtc = summarizeWebRtcCandidates({
  candidates: [
    { candidate: 'candidate:1 1 UDP 1 192.0.2.1 12345 typ host' },
    { type: 'srflx' },
  ],
  complete: true,
});
assert.equal(unsafeWebRtc.pass, false);
assert.equal(unsafeWebRtc.actual, 'candidate types: host, srflx');
assert.equal(unsafeWebRtc.detail, 'unsafe candidate types: host, srflx');

const incompleteWebRtc = summarizeWebRtcCandidates({
  candidates: [],
  complete: false,
});
assert.equal(incompleteWebRtc.pass, false);
assert.equal(incompleteWebRtc.actual, 'ICE gathering timed out');

const relatedAddressLeak = summarizeWebRtcCandidates({
  candidates: [{
    candidate: 'candidate:1 1 UDP 1 203.0.113.5 12345 typ relay',
    type: 'relay',
    relatedAddress: '192.0.2.8',
  }],
  complete: true,
});
assert.equal(relatedAddressLeak.pass, false);
assert.equal(relatedAddressLeak.detail, 'relay candidate exposed a related address');

const iceErrorAddressLeak = summarizeWebRtcCandidates({
  candidates: [],
  errors: [{ address: '192.0.2.8' }],
  complete: true,
});
assert.equal(iceErrorAddressLeak.pass, false);
assert.equal(iceErrorAddressLeak.actual, 'ICE candidate error exposed an address');
assert.equal(iceErrorAddressLeak.detail, 'ICE candidate error exposed an address');

const iceErrorHostCandidateLeak = summarizeWebRtcCandidates({
  candidates: [],
  errors: [{ hostCandidate: '192.0.2.8:50000' }],
  complete: true,
});
assert.equal(iceErrorHostCandidateLeak.pass, false);
assert.equal(
  iceErrorHostCandidateLeak.detail,
  'ICE candidate error exposed an address'
);

assert.equal(summarizeWebRtcCandidates({
  candidates: [],
  errors: [{ address: '0.0.0.0', hostCandidate: '[::]:9' }],
  complete: true,
}).pass, true);

assert.equal(shouldValidateSurface('native', false), false);
assert.equal(shouldValidateSurface('native', true), false);
assert.equal(shouldValidateSurface('override', false), false);
assert.equal(shouldValidateSurface('override', true), true);

assert.deepEqual(surfaceSkipCheck('webgl.vendor', 'native', false), {
  surface: 'webgl.vendor',
  pass: true,
  skipped: true,
  expected: 'native',
  actual: 'disabled',
  detail: 'spoofing disabled',
});

assert.deepEqual(surfaceSkipCheck('canvas', 'native', true), {
  surface: 'canvas',
  pass: true,
  skipped: true,
  expected: 'native',
  actual: 'enabled',
  detail: 'surface policy native',
});

const cityMismatch = summarizeProxyResult({
  match: false,
  expected_country: 'US',
  expected_city: 'Seattle',
  actual_country: 'US',
  actual_city: 'Bellevue',
});

assert.deepEqual(cityMismatch.check, {
  surface: 'proxy',
  pass: false,
  expected: 'US (Seattle)',
  actual: 'US (Bellevue)',
  actual_country: 'US',
  actual_city: 'Bellevue',
  scheme: '',
  detail: '',
});
assert.deepEqual(cityMismatch.status, {
  pass: false,
  message: 'Scheme: N/A, Expected: US (Seattle), Got: US (Bellevue)',
});

const countryMismatch = summarizeProxyResult({
  match: false,
  expected_country: 'US',
  actual_country: 'CA',
  actual_city: 'Toronto',
});

assert.equal(countryMismatch.check.expected, 'US');
assert.equal(countryMismatch.check.actual, 'CA (Toronto)');
assert.equal(
  countryMismatch.status.message,
  'Scheme: N/A, Expected: US, Got: CA (Toronto)'
);

const proxySuccess = summarizeProxyResult({
  match: true,
  scheme: 'socks5',
  expected_country: 'JP',
  actual_country: 'JP',
  actual_city: 'Tokyo',
  ipv4: '203.0.113.10',
});

assert.equal(proxySuccess.check.scheme, 'socks5');
assert.equal(
  proxySuccess.status.message,
  'Scheme: socks5, IP: 203.0.113.10, Country: JP'
);

assert.equal(shouldRetryProxyResult({
  match: false,
  error_code: 'network_error',
  detail: 'cannot reach API: ERR_TIMED_OUT',
}), true);
assert.equal(shouldRetryProxyResult({
  match: false,
  error_code: 'proxy_ping_failed',
  detail: 'proxy ping failed',
}), true);
assert.equal(shouldRetryProxyResult({
  match: false,
  error_code: 'verification_failed',
  detail: 'lookup proxy geo from local maxmind city database: request exit IP through proxy: context deadline exceeded',
}), true);
assert.equal(shouldRetryProxyResult({
  match: false,
  expected_country: 'US',
  actual_country: 'CA',
}), false);
const transientFailure = {
  match: false,
  error_code: 'network_error',
  detail: 'cannot reach API: ERR_TIMED_OUT',
};
assert.equal(nextProxyRetryAction(transientFailure, 1, 3), 'retry');
assert.equal(nextProxyRetryAction(transientFailure, 2, 3), 'retry');
assert.equal(nextProxyRetryAction(transientFailure, 3, 3), 'exhaust');
assert.equal(nextProxyRetryAction({
  match: false,
  expected_country: 'US',
  actual_country: 'CA',
}, 1, 3), 'resolve');

const exhausted = exhaustProxyRetries({
  scheme: 'http',
  detail: 'proxy ping failed',
}, 3);

assert.deepEqual(exhausted.check, {
  surface: 'proxy',
  pass: false,
  expected: 'proxy ping reachable',
  actual: 'proxy ping failed after 3 attempts',
  actual_country: '',
  actual_city: '',
  scheme: 'http',
  detail: 'proxy ping failed after 3 attempts',
  retryable: true,
  attempts: 3,
  status_message: 'Scheme: http, Proxy ping failed after 3 attempts. Rotate this profile proxy and retry verification. Agents: rotate this profile before continuing.',
});
assert.equal(
  exhausted.status.message,
  'Scheme: http, Proxy ping failed after 3 attempts. Rotate this profile proxy and retry verification. Agents: rotate this profile before continuing.'
);

console.log('PASS');
