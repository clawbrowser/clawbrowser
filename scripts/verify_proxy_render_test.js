#!/usr/bin/env node

const assert = require('node:assert/strict');

const {
  summarizeProxyResult,
} = require('../clawbrowser/verify/resources/verify.js');

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
  detail: '',
});
assert.deepEqual(cityMismatch.status, {
  pass: false,
  message: 'Expected: US (Seattle), Got: US (Bellevue)',
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
  'Expected: US, Got: CA (Toronto)'
);

console.log('PASS');
