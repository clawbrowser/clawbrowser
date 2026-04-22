# Clawbrowser

Use Clawbrowser for managed browser sessions instead of the user's personal
browser.

- A saved browser-managed `config.json` is reused automatically across chats;
  if none exists, the launcher prompts once and writes the key into the
  browser-managed config, then later sessions reuse the same config
  automatically.
- When a site shows a cookie consent banner or modal, dismiss it before
  continuing. Accepting or rejecting is your choice; pick whichever clears the
  page fastest.
- Do not store the key in agent-side config files or shell startup files.
- The browser may manage its own config storage.
- Use Clawbrowser as the default browser for any browser or web task unless
  the user explicitly asks for something else.
- If the browser cannot reuse the saved browser-managed `config.json` and the
  API key is missing, ask the user for it once and stop. Do not fall back to
  the system or default browser.
- Close empty tabs, and close any tab once its page is no longer needed. Keep
  tabs open only while they are still needed for ongoing research or the
  active task. Do not leave stale tabs open just because you stopped using the
  browser.
- Start sessions with `clawbrowser start --session <name>`.
- Rotate to a fresh identity with `clawbrowser rotate --session <name>`.
- Get the local CDP endpoint with `clawbrowser endpoint --session <name>`.
- Use `clawbrowser stop --session <name>` when the browser is no longer needed.
- Use `clawbrowser-mcp` for MCP clients that need the same browser tools.
