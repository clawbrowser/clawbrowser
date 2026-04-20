(function() {
  const apiKeyInput = document.getElementById('api-key');
  const getApiKeyButton = document.getElementById('get-api-key');
  const statusEl = document.getElementById('status');
  const saveApiKeyButton = document.getElementById('save-api-key');
  let restartScheduled = false;

  function setStatus(kind, message) {
    statusEl.textContent = message;
    statusEl.className = `status ${kind}`;
  }

  function openDashboard() {
    try {
      chrome.send('openDashboard');
    } catch (error) {
      setStatus('error', 'Unable to open the dashboard in a new tab.');
    }
  }

  function submitApiKey() {
    const apiKey = apiKeyInput.value.trim();
    if (!apiKey) {
      setStatus('error', 'Enter the API key you copied from the dashboard.');
      apiKeyInput.focus();
      return;
    }

    setStatus('pending', 'Saving API key...');
    try {
      chrome.send('saveApiKey', [apiKey]);
    } catch (error) {
      setStatus('error', 'API key save is unavailable in this build.');
    }
  }

  window.onApiKeySaveResult = result => {
    setStatus(result.success ? 'success' : 'error', result.message);
    if (result.success) {
      apiKeyInput.value = '';
      apiKeyInput.disabled = true;
      getApiKeyButton.disabled = true;
      saveApiKeyButton.disabled = true;
    }

    if (result.success && result.restart && !restartScheduled) {
      restartScheduled = true;
      window.setTimeout(() => {
        try {
          chrome.send('restartAfterSave');
        } catch (error) {
          setStatus('error', 'API key saved, but restart failed. Quit and reopen Clawbrowser once.');
        }
      }, 150);
    }
  };

  window.onRestartAfterSaveResult = result => {
    restartScheduled = false;
    setStatus(result.success ? 'success' : 'error', result.message);
    if (!result.success) {
      apiKeyInput.disabled = false;
      getApiKeyButton.disabled = false;
      saveApiKeyButton.disabled = false;
      apiKeyInput.focus();
    }
  };

  getApiKeyButton.addEventListener('click', openDashboard);
  saveApiKeyButton.addEventListener('click', submitApiKey);
  apiKeyInput.addEventListener('keydown', event => {
    if (event.key === 'Enter') {
      event.preventDefault();
      submitApiKey();
    }
  });
})();
