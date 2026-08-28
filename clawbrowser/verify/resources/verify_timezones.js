(function(root, factory) {
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
    return;
  }

  root.__clawbrowserVerifyTimeZones = factory();
})(typeof globalThis !== 'undefined' ? globalThis : this, function() {
  function canonicalizeTimeZone(value) {
    if (typeof value !== 'string') {
      return '';
    }

    const trimmed = value.trim();
    if (!trimmed) {
      return '';
    }

    try {
      return new Intl.DateTimeFormat('en-US', {
        timeZone: trimmed
      }).resolvedOptions().timeZone || trimmed;
    } catch (error) {
      return trimmed;
    }
  }

  function timeZonesMatch(expectedValue, actualValue) {
    const expectedCanonical = canonicalizeTimeZone(expectedValue);
    const actualCanonical = canonicalizeTimeZone(actualValue);

    return expectedCanonical !== '' &&
      actualCanonical !== '' &&
      expectedCanonical === actualCanonical;
  }

  return {
    canonicalizeTimeZone,
    timeZonesMatch
  };
});
