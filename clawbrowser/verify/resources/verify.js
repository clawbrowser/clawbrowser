// Clawbrowser fingerprint verification.
// Expected values injected by WebUI handler via window.__clawbrowser_expected.
// Results exposed via window.__clawbrowser_verify for CDP automation.

// Runtime-owned capability used by nextctl before it trusts a managed
// session. C++ derives it from the active fingerprint/proxy launch contract;
// it must never be inferred from the browser version alone.
function managedProxyPrivacyCapability(data) {
  const parsed = Number.parseInt(
    data && data.dataset.managedProxyPrivacy || '0', 10);
  return Number.isFinite(parsed) ? parsed : 0;
}

if (typeof window !== 'undefined' && typeof document !== 'undefined') {
  const capabilityData = document.getElementById('expected-data');
  window.__clawbrowser_capabilities = Object.freeze({
    managed_proxy_privacy: managedProxyPrivacyCapability(capabilityData),
  });
}

function formatProxyLocation(country, city) {
  const normalizedCountry = country || 'N/A';
  return city ? `${normalizedCountry} (${city})` : normalizedCountry;
}

function formatProxyScheme(scheme) {
  return scheme || 'N/A';
}

function isProxyPingFailure(result) {
  const code = String(result.error_code || '').toLowerCase();
  const detail = String(result.detail || '').toLowerCase();
  if (code === 'network_error' ||
      code === 'proxy_ping_failed' ||
      code === 'proxy_unreachable' ||
      code === 'proxy_timeout' ||
      code === 'upstream_timeout') {
    return true;
  }

  return /proxy/.test(detail) &&
    /(ping|reach|unreachable|timeout|timed out|connection|connect|context canceled|request exit ip)/.test(detail);
}

function shouldRetryProxyResult(result) {
  if (!result || result.match) {
    return false;
  }

  const code = String(result.error_code || '').toLowerCase();
  if (code === 'invalid_api_key' ||
      code === 'forbidden' ||
      code === 'rate_limited' ||
      code === 'parse_error') {
    return false;
  }

  return isProxyPingFailure(result);
}

function nextProxyRetryAction(result, attempts, maxAttempts) {
  if (!shouldRetryProxyResult(result)) {
    return 'resolve';
  }
  return attempts < maxAttempts ? 'retry' : 'exhaust';
}

function exhaustedProxyRetryMessage(scheme, attempts) {
  return `Scheme: ${formatProxyScheme(scheme)}, Proxy ping failed after ${attempts} attempts. ` +
    'Rotate this profile proxy and retry verification. ' +
    'Agents: rotate this profile before continuing.';
}

function exhaustProxyRetries(result, attempts) {
  const attemptCount = attempts || 1;
  const scheme = result.scheme || '';
  const detail = `proxy ping failed after ${attemptCount} attempts`;
  const message = exhaustedProxyRetryMessage(scheme, attemptCount);
  return {
    check: {
      surface: 'proxy',
      pass: false,
      expected: 'proxy ping reachable',
      actual: detail,
      actual_country: result.actual_country || '',
      actual_city: result.actual_city || '',
      scheme,
      detail,
      retryable: true,
      attempts: attemptCount,
      status_message: message,
    },
    status: {
      pass: false,
      message,
    }
  };
}

function summarizeProxyResult(result) {
  const scheme = result.scheme || '';
  const schemeText = `Scheme: ${formatProxyScheme(scheme)}`;
  const expectedValue = result.expected_country
    ? formatProxyLocation(result.expected_country, result.expected_city)
    : (result.detail || 'N/A');
  const actualValue = formatProxyLocation(
    result.actual_country || '',
    result.actual_city || ''
  );

  return {
    check: {
      surface: 'proxy',
      pass: Boolean(result.match),
      expected: expectedValue,
      actual: actualValue,
      actual_country: result.actual_country || '',
      actual_city: result.actual_city || '',
      scheme,
      detail: result.detail || ''
    },
    status: result.match
      ? {
          pass: true,
          message: `${schemeText}, IP: ${result.ipv4 || 'N/A'}, Country: ${result.actual_country || 'N/A'}`
        }
      : {
          pass: false,
          message: result.detail && actualValue === 'N/A'
            ? `${schemeText}, ${result.detail}`
            : `${schemeText}, Expected: ${expectedValue}, Got: ${actualValue}`
        }
  };
}

function normalizeSurfacePolicy(policyMode) {
  return policyMode || 'native';
}

function shouldValidateSurface(policyMode, spoofingEnabled) {
  return Boolean(spoofingEnabled) &&
    normalizeSurfacePolicy(policyMode) === 'override';
}

function surfaceSkipCheck(surface, policyMode, spoofingEnabled) {
  const normalizedPolicy = normalizeSurfacePolicy(policyMode);
  const enabled = Boolean(spoofingEnabled);
  return {
    surface,
    pass: true,
    skipped: true,
    expected: normalizedPolicy,
    actual: enabled ? 'enabled' : 'disabled',
    detail: enabled
      ? `surface policy ${normalizedPolicy}`
      : 'spoofing disabled',
  };
}

function iceCandidateType(candidate) {
  const explicitType = String(candidate && candidate.type || '').toLowerCase();
  if (explicitType) {
    return explicitType;
  }

  const candidateLine = String(candidate && candidate.candidate || candidate || '');
  const match = candidateLine.match(/\styp\s+(host|srflx|prflx|relay)(?:\s|$)/i);
  return match ? match[1].toLowerCase() : 'unknown';
}

function iceCandidateRelatedAddress(candidate) {
  const explicitAddress = String(
    candidate && candidate.relatedAddress || ''
  ).toLowerCase();
  if (explicitAddress) {
    return explicitAddress;
  }

  const candidateLine = String(candidate && candidate.candidate || candidate || '');
  const match = candidateLine.match(/\sraddr\s+([^\s]+)/i);
  return match ? match[1].toLowerCase() : '';
}

function sdpIceCandidates(sdp) {
  return String(sdp || '')
    .split(/\r?\n/)
    .filter(line => /^a=candidate:/i.test(line))
    .map(line => ({ candidate: line.slice(2), source: 'sdp' }));
}

function isUnspecifiedIceAddress(address) {
  const normalized = String(address || '').trim().toLowerCase();
  if (!normalized || normalized === '0.0.0.0' ||
      normalized === '::' || normalized === '[::]') {
    return true;
  }

  // RTCIceCandidateErrorEvent.hostCandidate is serialized as address:port.
  // Accept only the port-bearing forms of the same unspecified addresses.
  return /^0\.0\.0\.0:\d+$/.test(normalized) ||
    /^\[::\]:\d+$/.test(normalized) || /^:::\d+$/.test(normalized);
}

function summarizeWebRtcCandidates(result) {
  const normalizedCandidates = result && Array.isArray(result.candidates)
    ? result.candidates
    : [];
  const iceErrors = result && Array.isArray(result.errors)
    ? result.errors
    : [];
  const gatheringComplete = Boolean(result && result.complete);
  const candidateTypes = normalizedCandidates.map(iceCandidateType);
  const unsafeTypes = candidateTypes.filter(type => type !== 'relay');
  const relatedAddresses = normalizedCandidates
    .map(iceCandidateRelatedAddress)
    .filter(address => !isUnspecifiedIceAddress(address));
  const exposedErrorAddresses = iceErrors.flatMap(error => [
    error && error.address,
    error && error.hostCandidate,
  ]).filter(address => !isUnspecifiedIceAddress(address));
  const pass = gatheringComplete && unsafeTypes.length === 0 &&
    relatedAddresses.length === 0 && exposedErrorAddresses.length === 0;
  let actual = candidateTypes.length === 0
    ? 'gathering complete; no ICE candidates exposed'
    : `candidate types: ${Array.from(new Set(candidateTypes)).join(', ')}`;

  let detail = 'no direct or related address exposed';
  if (unsafeTypes.length > 0) {
    detail = `unsafe candidate types: ${Array.from(new Set(unsafeTypes)).join(', ')}`;
  } else if (relatedAddresses.length > 0) {
    detail = 'relay candidate exposed a related address';
  } else if (exposedErrorAddresses.length > 0) {
    actual = 'ICE candidate error exposed an address';
    detail = 'ICE candidate error exposed an address';
  } else if (!gatheringComplete) {
    actual = 'ICE gathering timed out';
    detail = 'ICE gathering did not complete';
  }

  return {
    surface: 'webrtc.iceCandidates',
    pass,
    expected: 'completed gathering; relay candidates only; no related or ICE error address',
    actual,
    detail,
  };
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    exhaustProxyRetries,
    formatProxyLocation,
    formatProxyScheme,
    managedProxyPrivacyCapability,
    nextProxyRetryAction,
    iceCandidateType,
    iceCandidateRelatedAddress,
    isUnspecifiedIceAddress,
    sdpIceCandidates,
    shouldRetryProxyResult,
    shouldValidateSurface,
    summarizeWebRtcCandidates,
    surfaceSkipCheck,
    summarizeProxyResult
  };
}

if (typeof document !== 'undefined') {
  (async function() {
  const data = document.getElementById('expected-data');
  const expected = !data || data.dataset.hasExpectedValues !== 'true'
    ? null
    : {
        has_proxy: data.dataset.hasProxy === 'true',
        user_agent: data.dataset.userAgent || '',
        platform: data.dataset.platform || '',
        language_primary: data.dataset.languagePrimary || '',
        languages_json: data.dataset.languagesJson || '[]',
        hardware_concurrency: data.dataset.hardwareConcurrency || '0',
        device_memory: data.dataset.deviceMemory || '0',
        screen_width: data.dataset.screenWidth || '0',
        screen_height: data.dataset.screenHeight || '0',
        screen_avail_width: data.dataset.screenAvailWidth || '0',
        screen_avail_height: data.dataset.screenAvailHeight || '0',
        screen_color_depth: data.dataset.screenColorDepth || '0',
        pixel_ratio: data.dataset.pixelRatio || '0',
        timezone: data.dataset.timezone || '',
        canvas_policy: data.dataset.canvasPolicy || 'native',
        canvas_spoofing_enabled: data.dataset.canvasSpoofingEnabled === 'true',
        webgl_policy: data.dataset.webglPolicy || 'native',
        webgl_spoofing_enabled: data.dataset.webglSpoofingEnabled === 'true',
        webgl_vendor: data.dataset.webglVendor || '',
        webgl_renderer: data.dataset.webglRenderer || '',
        fonts: data.dataset.fonts || '[]',
        media_devices_json: data.dataset.mediaDevicesJson || '',
        plugins_json: data.dataset.pluginsJson || '',
        battery_charging: data.dataset.batteryCharging || '',
        battery_level: data.dataset.batteryLevel || '',
        speech_voices_json: data.dataset.speechVoicesJson || ''
      };

  window.__clawbrowser_expected = expected;
  if (!expected) {
    document.getElementById('status').textContent =
      'No expected values — not running in fingerprint mode';
    document.getElementById('status').className = 'fail';
    return;
  }

  const checkMap = new Map();
  const pendingAsyncChecks = new Set();
  let syncChecksComplete = false;
  let finalizationDone = false;
  let proxyVerifyAttempts = 0;
  let proxyStatus = {
    pass: true,
    message: 'skipped'
  };

  const maxProxyVerifyAttempts = 3;

  function createCell(text, className) {
    const cell = document.createElement('td');
    if (className) {
      cell.className = className;
    }
    cell.textContent = text || '';
    return cell;
  }

  function renderProxyStatus() {
    const proxyStatusEl = document.getElementById('proxy-status');
    proxyStatusEl.replaceChildren();

    const statusLabel = document.createElement('span');
    statusLabel.className = proxyStatus.pass ? 'check-pass' : 'check-fail';
    statusLabel.textContent = `Proxy: ${proxyStatus.pass ? 'PASS' : 'FAIL'}`;
    proxyStatusEl.append(
      statusLabel,
      document.createTextNode(` — ${proxyStatus.message}`)
    );
  }

  function setCheck(result) {
    if (finalizationDone) {
      return;
    }
    checkMap.set(result.surface, result);
  }

  function beginAsyncCheck(name) {
    pendingAsyncChecks.add(name);
  }

  function completeAsyncCheck(name) {
    pendingAsyncChecks.delete(name);
    finalizeIfReady();
  }

  function check(surface, expectedVal, actualVal) {
    const pass = String(expectedVal) === String(actualVal);
    setCheck({
      surface,
      pass,
      expected: String(expectedVal),
      actual: String(actualVal)
    });
  }

  function checkTimeZone(surface, expectedVal, actualVal) {
    const helper = globalThis.__clawbrowserVerifyTimeZones;
    const pass = helper && typeof helper.timeZonesMatch === 'function'
      ? helper.timeZonesMatch(expectedVal, actualVal)
      : String(expectedVal) === String(actualVal);

    setCheck({
      surface,
      pass,
      expected: String(expectedVal),
      actual: String(actualVal)
    });
  }

  function normalizeJsonValue(value) {
    if (Array.isArray(value)) {
      return value.map(normalizeJsonValue);
    }
    if (value && typeof value === 'object') {
      const normalized = {};
      for (const key of Object.keys(value).sort()) {
        normalized[key] = normalizeJsonValue(value[key]);
      }
      return normalized;
    }
    return value;
  }

  function checkJson(surface, expectedJson, actualValue) {
    try {
      const normalizedExpected = JSON.stringify(
        normalizeJsonValue(JSON.parse(expectedJson))
      );
      const normalizedActual = JSON.stringify(
        normalizeJsonValue(actualValue)
      );
      setCheck({
        surface,
        pass: normalizedExpected === normalizedActual,
        expected: normalizedExpected,
        actual: normalizedActual
      });
    } catch (e) {
      setCheck({
        surface,
        pass: false,
        expected: expectedJson,
        actual: JSON.stringify(actualValue),
        detail: e.message
      });
    }
  }

  window.__clawbrowser_verify_helpers = {
    sameJson(expectedJson, actualValue) {
      try {
        const normalizedExpected = JSON.stringify(
          normalizeJsonValue(JSON.parse(expectedJson))
        );
        const normalizedActual = JSON.stringify(
          normalizeJsonValue(actualValue)
        );
        return normalizedExpected === normalizedActual;
      } catch (e) {
        return false;
      }
    }
  };

  function finalizeIfReady() {
    if (finalizationDone || !syncChecksComplete || pendingAsyncChecks.size > 0) {
      return;
    }

    finalizationDone = true;

    const finalChecks = Array.from(checkMap.values());
    const tbody = document.getElementById('results-body');
    tbody.replaceChildren();

    for (const checkResult of finalChecks) {
      const row = document.createElement('tr');
      const statusText = checkResult.skipped
        ? 'SKIP'
        : (checkResult.pass ? 'PASS' : 'FAIL');
      const statusClass = checkResult.skipped
        ? 'check-skip'
        : (checkResult.pass ? 'check-pass' : 'check-fail');
      row.append(
        createCell(checkResult.surface),
        createCell(statusText, statusClass),
        createCell(checkResult.expected || checkResult.detail || ''),
        createCell(checkResult.actual || '')
      );
      tbody.appendChild(row);
    }

    const allPass = finalChecks.every(checkResult => checkResult.pass);
    const hasSkipped = finalChecks.some(checkResult => checkResult.skipped);
    const proxyFailure = finalChecks.find(checkResult =>
      checkResult.surface === 'proxy' &&
      !checkResult.pass &&
      checkResult.status_message
    );
    const statusEl = document.getElementById('status');
    statusEl.textContent = allPass
      ? (hasSkipped ? 'All active checks passed' : 'All checks passed')
      : (proxyFailure ? proxyFailure.status_message : 'Some checks failed');
    statusEl.className = allPass ? 'pass' : 'fail';

    renderProxyStatus();

    window.__clawbrowser_verify = {
      status: allPass ? 'pass' : 'fail',
      checks: finalChecks.map(checkResult => ({ ...checkResult })),
      timestamp: new Date().toISOString()
    };

    try {
      chrome.send('verifyComplete', [window.__clawbrowser_verify.status]);
    } catch (e) {
      // chrome.send not available outside WebUI context
    }
  }

  function resolveProxyResult(result) {
    const summary = result && result.__exhausted_proxy_retries
      ? exhaustProxyRetries(result, result.attempts)
      : summarizeProxyResult(result);
    setCheck(summary.check);
    proxyStatus = summary.status;

    completeAsyncCheck('proxy');
  }

  window.onProxyVerifyResult = result => {
    const normalizedResult = {
      ...(result || {}),
      attempts: proxyVerifyAttempts
    };
    const retryAction = nextProxyRetryAction(
      normalizedResult,
      proxyVerifyAttempts,
      maxProxyVerifyAttempts
    );
    if (retryAction === 'retry') {
      setTimeout(requestProxyVerification, proxyRetryDelayMs(proxyVerifyAttempts));
      return;
    }

    if (retryAction === 'exhaust') {
      resolveProxyResult({
        ...normalizedResult,
        __exhausted_proxy_retries: true
      });
      return;
    }

    resolveProxyResult(normalizedResult);
  };

  function proxyRetryDelayMs(attempt) {
    return Math.min(Math.max(attempt, 1) * 1000, 2000);
  }

  function requestProxyVerification() {
    proxyVerifyAttempts += 1;
    try {
      chrome.send('verifyProxy');
    } catch (e) {
      resolveProxyResult({
        match: true,
        actual_country: 'N/A',
        detail: 'proxy verification unavailable',
      });
    }
  }

  function collectIceCandidates(iceServers) {
    return new Promise((resolve, reject) => {
      const peerConnection = new RTCPeerConnection({ iceServers });
      const candidates = [];
      const errors = [];
      let resolved = false;
      let timeoutId;
      const finish = async complete => {
        if (resolved) {
          return;
        }
        resolved = true;
        clearTimeout(timeoutId);
        const sdpCandidates = sdpIceCandidates(
          peerConnection.localDescription && peerConnection.localDescription.sdp
        );
        const statsCandidates = [];
        let statsComplete = true;
        let statsTimeoutId;
        try {
          const stats = await Promise.race([
            peerConnection.getStats(),
            new Promise((_, reject) => {
              statsTimeoutId = setTimeout(
                () => reject(new Error('getStats timed out')),
                2000
              );
            }),
          ]);
          stats.forEach(report => {
            if (report.type !== 'local-candidate') {
              return;
            }
            statsCandidates.push({
              candidate: '',
              type: report.candidateType || '',
              address: report.address || report.ip || '',
              relatedAddress: report.relatedAddress || '',
              source: 'stats',
            });
          });
        } catch (error) {
          statsComplete = false;
        } finally {
          clearTimeout(statsTimeoutId);
        }
        peerConnection.close();
        resolve({
          candidates: [
            ...candidates,
            ...sdpCandidates,
            ...statsCandidates,
          ],
          errors,
          complete: complete && statsComplete,
        });
      };

      peerConnection.onicecandidate = event => {
        if (!event.candidate) {
          if (peerConnection.iceGatheringState === 'complete') {
            void finish(true);
          }
          return;
        }
        candidates.push({
          candidate: event.candidate.candidate || '',
          type: event.candidate.type || '',
          address: event.candidate.address || '',
          relatedAddress: event.candidate.relatedAddress || '',
          source: 'event',
        });
      };
      peerConnection.onicecandidateerror = event => {
        errors.push({
          address: event.address || '',
          hostCandidate: event.hostCandidate || '',
          port: event.port || 0,
        });
      };
      peerConnection.onicegatheringstatechange = () => {
        if (peerConnection.iceGatheringState === 'complete') {
          void finish(true);
        }
      };

      peerConnection.createDataChannel('clawbrowser-webrtc-verify');
      peerConnection.createOffer()
        .then(offer => peerConnection.setLocalDescription(offer))
        .catch(error => {
          clearTimeout(timeoutId);
          peerConnection.close();
          reject(error);
        });
      timeoutId = setTimeout(() => void finish(false), 7000);
    });
  }

  async function verifyWebRtcCandidates() {
    try {
      // The built-in diagnostic must not contact a third-party STUN service.
      // Controlled STUN/TURN endpoints belong in opt-in integration tests.
      const observation = await collectIceCandidates([]);
      setCheck(summarizeWebRtcCandidates(observation));
    } catch (error) {
      setCheck({
        surface: 'webrtc.iceCandidates',
        pass: false,
        expected: 'completed gathering; relay candidates only; no related or ICE error address',
        actual: 'check failed',
        detail: error && error.message ? error.message : String(error),
      });
    }
    completeAsyncCheck('webrtc');
  }

  // Navigator
  check('navigator.userAgent', expected.user_agent, navigator.userAgent);
  check('navigator.platform', expected.platform, navigator.platform);
  check('navigator.language', expected.language_primary, navigator.language);
  check('navigator.languages', expected.languages_json, JSON.stringify(navigator.languages));
  check('navigator.hardwareConcurrency', expected.hardware_concurrency, navigator.hardwareConcurrency);
  check('navigator.deviceMemory', expected.device_memory, navigator.deviceMemory);

  // Screen
  check('screen.width', expected.screen_width, screen.width);
  check('screen.height', expected.screen_height, screen.height);
  check('screen.availWidth', expected.screen_avail_width, screen.availWidth);
  check('screen.availHeight', expected.screen_avail_height, screen.availHeight);
  check('screen.availLeft', 0, screen.availLeft);
  check('screen.availTop', 0, screen.availTop);
  check('window.screenX', 0, window.screenX);
  check('window.screenY', 0, window.screenY);
  check('window.screenLeft', 0, window.screenLeft);
  check('window.screenTop', 0, window.screenTop);
  check('screen.isExtended', false, screen.isExtended);
  check('screen.colorDepth', expected.screen_color_depth, screen.colorDepth);
  check('window.devicePixelRatio', expected.pixel_ratio, window.devicePixelRatio);
  const expectedOrientation = expected.screen_height >= expected.screen_width
    ? 'portrait-primary'
    : 'landscape-primary';
  check('screen.orientation.type', expectedOrientation, screen.orientation.type);
  check('screen.orientation.angle', 0, screen.orientation.angle);
  if ('orientation' in window) {
    check('window.orientation', 0, window.orientation);
  }

  // Timezone
  checkTimeZone(
    'timezone',
    expected.timezone,
    Intl.DateTimeFormat().resolvedOptions().timeZone
  );

  function recordDeterminism(surface, hash1, hash2) {
    const pass = hash1 === hash2;
    setCheck({
      surface,
      pass,
      detail: pass ? 'deterministic' : 'non-deterministic',
      expected: hash1,
      actual: hash2
    });
  }

  async function deterministicCheck(surface, fn) {
    const hash1 = await fn();
    const hash2 = await fn();
    recordDeterminism(surface, hash1, hash2);
  }

  async function hashArrayBuffer(buffer) {
    const hashBuffer = await crypto.subtle.digest('SHA-256', buffer);
    return Array.from(new Uint8Array(hashBuffer))
      .map(byte => byte.toString(16).padStart(2, '0'))
      .join('');
  }

  async function canvasHash() {
    const canvas = document.createElement('canvas');
    canvas.width = 200;
    canvas.height = 50;
    const ctx = canvas.getContext('2d');
    ctx.textBaseline = 'top';
    ctx.font = '14px Arial';
    ctx.fillStyle = '#f60';
    ctx.fillRect(125, 1, 62, 20);
    ctx.fillStyle = '#069';
    ctx.fillText('Clawbrowser test', 2, 15);
    ctx.fillStyle = 'rgba(102, 204, 0, 0.7)';
    ctx.fillText('Clawbrowser test', 4, 17);
    return hashArrayBuffer(new TextEncoder().encode(canvas.toDataURL()));
  }
  if (shouldValidateSurface(
      expected.canvas_policy,
      expected.canvas_spoofing_enabled)) {
    await deterministicCheck('canvas', canvasHash);
  } else {
    setCheck(surfaceSkipCheck(
      'canvas',
      expected.canvas_policy,
      expected.canvas_spoofing_enabled));
  }

  async function webglHash() {
    const canvas = document.createElement('canvas');
    const gl = canvas.getContext('webgl');
    if (!gl) {
      return 'no-webgl';
    }

    canvas.width = 64;
    canvas.height = 64;
    gl.clearColor(0.5, 0.3, 0.1, 1.0);
    gl.clear(gl.COLOR_BUFFER_BIT);
    const pixels = new Uint8Array(64 * 64 * 4);
    gl.readPixels(0, 0, 64, 64, gl.RGBA, gl.UNSIGNED_BYTE, pixels);
    return hashArrayBuffer(pixels.buffer);
  }
  if (shouldValidateSurface(
      expected.webgl_policy,
      expected.webgl_spoofing_enabled)) {
    await deterministicCheck('webgl.readPixels', webglHash);

    const glCanvas = document.createElement('canvas');
    const gl = glCanvas.getContext('webgl');
    if (gl) {
      const debugExt = gl.getExtension('WEBGL_debug_renderer_info');
      if (debugExt) {
        check('webgl.vendor', expected.webgl_vendor,
          gl.getParameter(debugExt.UNMASKED_VENDOR_WEBGL));
        check('webgl.renderer', expected.webgl_renderer,
          gl.getParameter(debugExt.UNMASKED_RENDERER_WEBGL));
      }
    }
  } else {
    setCheck(surfaceSkipCheck(
      'webgl.readPixels',
      expected.webgl_policy,
      expected.webgl_spoofing_enabled));
    setCheck(surfaceSkipCheck(
      'webgl.vendor',
      expected.webgl_policy,
      expected.webgl_spoofing_enabled));
    setCheck(surfaceSkipCheck(
      'webgl.renderer',
      expected.webgl_policy,
      expected.webgl_spoofing_enabled));
  }

  async function audioHash() {
    const ctx = new OfflineAudioContext(1, 44100, 44100);
    const oscillator = ctx.createOscillator();
    oscillator.type = 'triangle';
    oscillator.frequency.setValueAtTime(10000, ctx.currentTime);
    const compressor = ctx.createDynamicsCompressor();
    oscillator.connect(compressor);
    compressor.connect(ctx.destination);
    oscillator.start(0);
    const buffer = await ctx.startRendering();
    return hashArrayBuffer(buffer.getChannelData(0).buffer);
  }
  await deterministicCheck('audio', audioHash);

  async function clientRectsHash() {
    const element = document.createElement('div');
    element.style.cssText =
      'position:absolute;top:10px;left:10px;width:100px;height:50px;';
    document.body.appendChild(element);
    const rect = element.getBoundingClientRect();
    document.body.removeChild(element);
    return `${rect.x},${rect.y},${rect.width},${rect.height}`;
  }
  await deterministicCheck('clientRects', clientRectsHash);

  if (expected.fonts) {
    const fontsExpected = JSON.parse(expected.fonts);
    const detectedFonts = [];
    const missingFonts = [];
    for (const font of fontsExpected) {
      const testSpan = document.createElement('span');
      testSpan.style.fontFamily = `"${font}", monospace`;
      testSpan.textContent = 'mmmmmmmmmmlli';
      document.body.appendChild(testSpan);
      const width = testSpan.offsetWidth;
      document.body.removeChild(testSpan);

      const monoSpan = document.createElement('span');
      monoSpan.style.fontFamily = 'monospace';
      monoSpan.textContent = 'mmmmmmmmmmlli';
      document.body.appendChild(monoSpan);
      const monoWidth = monoSpan.offsetWidth;
      document.body.removeChild(monoSpan);

      if (width !== monoWidth) {
        detectedFonts.push(font);
      } else {
        missingFonts.push(font);
      }
    }

    setCheck({
      surface: 'fonts',
      pass: missingFonts.length === 0,
      expected: JSON.stringify(fontsExpected),
      actual: JSON.stringify(detectedFonts),
      detail: `${missingFonts.length} missing`
    });
  }

  if (expected.media_devices_json !== '') {
    try {
      const devices = await navigator.mediaDevices.enumerateDevices();
      const actualMediaDevices = devices.map(device => ({
        kind: device.kind || '',
        label: device.label || '',
        device_id: device.deviceId || ''
      }));
      checkJson('mediaDevices', expected.media_devices_json, actualMediaDevices);
    } catch (e) {
      setCheck({
        surface: 'mediaDevices',
        pass: false,
        expected: expected.media_devices_json,
        actual: '[]',
        detail: e.message
      });
    }
  }

  if (expected.plugins_json !== '') {
    const actualPlugins = Array.from(navigator.plugins).map(plugin => ({
      name: plugin.name || '',
      description: plugin.description || '',
      filename: plugin.filename || ''
    }));
    checkJson('plugins', expected.plugins_json, actualPlugins);
  }

  const hasExpectedBatteryCharging = expected.battery_charging !== '';
  const hasExpectedBatteryLevel = expected.battery_level !== '';
  if (hasExpectedBatteryCharging || hasExpectedBatteryLevel) {
    try {
      const battery = await navigator.getBattery();
      if (hasExpectedBatteryCharging) {
        check('battery.charging', expected.battery_charging, battery.charging);
      }
      if (hasExpectedBatteryLevel) {
        check('battery.level', expected.battery_level, battery.level);
      }
    } catch (e) {
      setCheck({ surface: 'battery', pass: false, detail: e.message });
    }
  }

  function resolveVoices(voices) {
    const actualVoices = voices.map(voice => voice.name);
    check(
      'speechSynthesis.voices',
      expected.speech_voices_json,
      JSON.stringify(actualVoices)
    );
    speechSynthesis.onvoiceschanged = null;
    completeAsyncCheck('speech');
  }

  if (expected.speech_voices_json !== '') {
    beginAsyncCheck('speech');
    const voices = speechSynthesis.getVoices();
    if (voices.length > 0) {
      resolveVoices(voices);
    } else {
      let voicesResolved = false;
      const finishVoices = resolvedVoices => {
        if (voicesResolved) {
          return;
        }
        voicesResolved = true;
        resolveVoices(resolvedVoices);
      };

      speechSynthesis.onvoiceschanged = () => {
        finishVoices(speechSynthesis.getVoices());
      };

      setTimeout(() => {
        finishVoices(speechSynthesis.getVoices());
      }, 3000);
    }
  }

  if (expected.has_proxy) {
    beginAsyncCheck('webrtc');
    void verifyWebRtcCandidates();
  }

  beginAsyncCheck('proxy');
  requestProxyVerification();

  syncChecksComplete = true;
  finalizeIfReady();
  })();
}
