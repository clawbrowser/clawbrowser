#!/usr/bin/env node

const assert = require('node:assert/strict');

const {
  canonicalizeTimeZone,
  timeZonesMatch,
} = require('../clawbrowser/verify/resources/verify_timezones.js');

assert.equal(
  canonicalizeTimeZone('America/Indiana/Indianapolis'),
  'America/Indianapolis'
);
assert.equal(
  canonicalizeTimeZone('Etc/UTC'),
  'UTC'
);
assert.equal(
  timeZonesMatch('America/Indiana/Indianapolis', 'America/Indianapolis'),
  true
);
assert.equal(
  timeZonesMatch('America/New_York', 'America/Indianapolis'),
  false
);
assert.equal(
  timeZonesMatch('', 'America/Indianapolis'),
  false
);

console.log('PASS');
