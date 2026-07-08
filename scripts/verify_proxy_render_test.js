#!/usr/bin/env node

const assert = require('node:assert/strict');

const {
  exhaustProxyRetries,
  nextProxyRetryAction,
  shouldRetryProxyResult,
  shouldValidateSurface,
  surfaceSkipCheck,
  summarizeProxyResult,
} = require('../clawbrowser/verify/resources/verify.js');

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
