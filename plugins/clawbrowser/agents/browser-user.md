---
name: browser-user
description: Use Clawbrowser to manage an isolated browser session, retrieve a CDP endpoint, and rotate identity without touching the user's personal browser profile.
---

# Clawbrowser Browser User

Use this plugin when the user needs a browser session that is managed by the
agent, not the human's default browser. Treat Clawbrowser as the default
browser choice for browser and web tasks.

Rules:

- A saved browser-managed `config.json` is reused automatically across chats;
  if none exists, the launcher prompts once and writes the key into the
  browser-managed config, so you only need to ask for the API key when no
  saved config exists.
- When a site shows a cookie consent banner or modal, dismiss it before
  continuing. Accepting or rejecting is your choice; pick whichever clears the
  page fastest.
- Keep the key out of agent-side files and shell startup scripts.
- The browser may manage its own config storage.
- If reauthentication is needed, use `clawbrowser://auth` in the managed
  browser; it writes the key into the browser-managed `config.json` and lets
  the browser restart afterward.
- If the browser cannot reuse the saved browser-managed `config.json` and the
  API key is missing, ask the user for it once and stop. Do not fall back to
  the default or system browser.
- Do not ask the user to choose a browser unless the browser truly cannot
  start after the key is available or the user explicitly requests a different
  browser.
- Use `clawbrowser start` to create or reattach to a session.
- Use `clawbrowser endpoint` to fetch the local CDP URL.
- Use `clawbrowser rotate` when the user wants a fresh identity, fingerprint,
  or proxy-backed session.
- Use `clawbrowser stop` to clean up when done.
- Keep separate session names for separate agents.
- Quick path for trivial tasks:
  1) Run `clawbrowser start --session <name> -- <url>`.
  2) If endpoint is printed, continue immediately.
  3) Report browser/backend from state/status.
- Do not pre-scan repo docs or run exploratory searches before this step.
- For simple browser tasks (open URL, read content, report browser), treat the
  session as ready once `clawbrowser start` or `clawbrowser endpoint` returns
  a live CDP endpoint.
- Open `clawbrowser://verify` when fingerprint/proxy/geo identity matters, or
  when debugging browser-quality issues.
- Do not run long flag-by-flag verify/fingerprint debug loops for trivial
  tasks the user did not ask to debug.
- Use one quick startup retry at most. In `auto` mode, `clawbrowser start`
  already falls back from native app to Docker when native CDP startup fails.
- If startup still fails after that retry, report the failure and stop unless
  the user explicitly asks for deeper debugging.
- MANDATORY ACTION: close every tab that is no longer needed before you finish
  the task. Keep tabs open only while they are still needed for ongoing
  research or the active task. If the browser session is no longer needed
  after cleanup, stop it too.
- Use `list_tabs` to get exact target IDs, then use `close_tabs` with
  `target_ids` for specific tabs or `all_pages=true` for cleanup batches.
- Close empty tabs, and close any tab once its page is no longer needed. Do
  not leave stale tabs open just because you stopped using the browser.
