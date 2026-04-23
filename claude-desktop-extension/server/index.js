#!/usr/bin/env node
"use strict";

const fs = require("fs");
const os = require("os");
const path = require("path");
const { spawn, spawnSync } = require("child_process");

const SERVER_NAME = "clawbrowser";
const SERVER_VERSION = "1.0.0";

function log(message) {
  process.stderr.write(`[clawbrowser-mcp] ${message}\n`);
}

function launcherPath() {
  const envPath = process.env.CLAWBROWSER_BIN;
  if (envPath) {
    return envPath;
  }

  const which = spawnSync("which", ["clawbrowser"], { encoding: "utf8" });
  if (which.status === 0) {
    const resolved = (which.stdout || "").trim();
    if (resolved) {
      return resolved;
    }
  }

  const local = path.resolve(__dirname, "..", "bin", "clawbrowser");
  if (fs.existsSync(local)) {
    return local;
  }

  throw new Error(
    "Could not find the clawbrowser launcher. Set CLAWBROWSER_BIN or install clawbrowser on PATH.",
  );
}

function launcherInvocation(args) {
  const launcher = launcherPath();
  try {
    fs.accessSync(launcher, fs.constants.X_OK);
    return { command: launcher, args };
  } catch (_err) {
    return { command: "bash", args: [launcher, ...args] };
  }
}

function configHomeDir() {
  return process.env.XDG_CONFIG_HOME || path.join(os.homedir(), ".config");
}

function savedConfigPath() {
  return path.join(configHomeDir(), "clawbrowser", "config.json");
}

function savedConfigHasApiKey() {
  try {
    const raw = fs.readFileSync(savedConfigPath(), "utf8");
    const payload = JSON.parse(raw);
    const apiKey = payload.api_key;
    return typeof apiKey === "string" && apiKey.trim().length > 0;
  } catch (_err) {
    return false;
  }
}

function textResult(text) {
  return { content: [{ type: "text", text }] };
}

function errorResult(text) {
  return { content: [{ type: "text", text }], isError: true };
}

function normalizeEndpoint(endpoint) {
  const trimmed = String(endpoint || "").trim().replace(/\/+$/, "");
  try {
    const parsed = new URL(trimmed);
    if (parsed.protocol === "ws:") {
      return `http://${parsed.host}`;
    }
    if (parsed.protocol === "wss:") {
      return `https://${parsed.host}`;
    }
    return trimmed;
  } catch (_err) {
    return trimmed;
  }
}

function normalizeSession(argumentsObj) {
  const value = argumentsObj?.session || argumentsObj?.session_name || "default";
  return String(value);
}

function runLauncher(args, options = {}) {
  const env = { ...process.env };
  if (options.apiKey) {
    env.CLAWBROWSER_API_KEY = String(options.apiKey);
  }
  if (options.image) {
    env.CLAWBROWSER_IMAGE = String(options.image);
  }

  const invocation = launcherInvocation(args);
  const proc = spawnSync(invocation.command, invocation.args, {
    encoding: "utf8",
    env,
  });

  return {
    code: typeof proc.status === "number" ? proc.status : 1,
    stdout: (proc.stdout || "").trim(),
    stderr: (proc.stderr || "").trim(),
  };
}

let runtimeBootstrapStarted = false;

function triggerRuntimeBootstrap() {
  if (runtimeBootstrapStarted) {
    return;
  }
  runtimeBootstrapStarted = true;

  let invocation;
  try {
    invocation = launcherInvocation(["ensure-runtime"]);
  } catch (err) {
    log(`Runtime bootstrap skipped: ${err.message || String(err)}`);
    return;
  }

  const child = spawn(invocation.command, invocation.args, {
    env: { ...process.env },
    stdio: "ignore",
  });

  child.on("error", (err) => {
    log(`Runtime bootstrap failed to start: ${err.message || String(err)}`);
  });
}

function bootstrapError() {
  const filePath = savedConfigPath();
  return errorResult(
    `Clawbrowser can reuse the saved browser config at ${filePath}. `
      + "If it is missing, provide the optional API key in the Claude Desktop "
      + "extension settings or open clawbrowser://auth after launching Clawbrowser once.",
  );
}

function getSessionEndpoint(argumentsObj) {
  const result = runLauncher(["endpoint", "--session", normalizeSession(argumentsObj)]);
  if (result.code !== 0) {
    return {
      endpoint: null,
      error: result.stderr || result.stdout || `launcher exited with code ${result.code}`,
    };
  }

  const endpoint = result.stdout.split("\n").filter(Boolean).pop() || "";
  if (!endpoint) {
    return { endpoint: null, error: "launcher did not return a CDP endpoint" };
  }
  return { endpoint, error: null };
}

async function fetchWithTimeout(url, options = {}) {
  if (typeof fetch !== "function") {
    throw new Error("This Node runtime does not provide fetch().");
  }
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 2000);
  try {
    const response = await fetch(url, { ...options, signal: controller.signal });
    return response;
  } finally {
    clearTimeout(timeout);
  }
}

async function fetchJson(url) {
  const response = await fetchWithTimeout(url);
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  return response.json();
}

function sessionHttpEndpoint(endpoint) {
  return normalizeEndpoint(endpoint);
}

async function fetchPageTargets(endpoint) {
  const payload = await fetchJson(`${sessionHttpEndpoint(endpoint)}/json/list`);
  if (!Array.isArray(payload)) {
    throw new Error("unexpected /json/list payload");
  }
  return payload.filter((target) => target && target.type === "page");
}

async function closePageTarget(endpoint, targetId) {
  const url = `${sessionHttpEndpoint(endpoint)}/json/close/${encodeURIComponent(String(targetId))}`;
  const response = await fetchWithTimeout(url);
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  return (await response.text()).trim();
}

function startSession(argumentsObj) {
  const apiKey = argumentsObj?.api_key || process.env.CLAWBROWSER_API_KEY;
  if (!apiKey && !savedConfigHasApiKey()) {
    return bootstrapError();
  }

  const args = ["start", "--session", normalizeSession(argumentsObj)];
  if (argumentsObj?.port !== undefined) {
    args.push("--port", String(argumentsObj.port));
  }
  if (argumentsObj?.image) {
    args.push("--image", String(argumentsObj.image));
  }
  if (argumentsObj?.verify_automation || argumentsObj?.verify) {
    args.push("--verify-automation");
  }
  if (argumentsObj?.fingerprint) {
    args.push("--fingerprint", String(argumentsObj.fingerprint));
  }
  if (argumentsObj?.regenerate) {
    args.push("--regenerate");
  }
  if (argumentsObj?.country) {
    args.push(`--country=${argumentsObj.country}`);
  }
  if (argumentsObj?.city) {
    args.push(`--city=${argumentsObj.city}`);
  }
  if (argumentsObj?.connection_type) {
    args.push(`--connection-type=${argumentsObj.connection_type}`);
  }

  const result = runLauncher(args, { apiKey, image: argumentsObj?.image });
  if (result.code !== 0) {
    return errorResult(result.stderr || result.stdout || `launcher exited with code ${result.code}`);
  }

  const endpoint = result.stdout.split("\n").filter(Boolean).pop() || "";
  return textResult(endpoint || result.stdout);
}

function rotateSession(argumentsObj) {
  const apiKey = argumentsObj?.api_key || process.env.CLAWBROWSER_API_KEY;
  if (!apiKey && !savedConfigHasApiKey()) {
    return bootstrapError();
  }

  const args = ["rotate", "--session", normalizeSession(argumentsObj)];
  if (argumentsObj?.image) {
    args.push("--image", String(argumentsObj.image));
  }

  const result = runLauncher(args, { apiKey, image: argumentsObj?.image });
  if (result.code !== 0) {
    return errorResult(result.stderr || result.stdout || `launcher exited with code ${result.code}`);
  }

  const endpoint = result.stdout.split("\n").filter(Boolean).pop() || "";
  return textResult(endpoint || result.stdout);
}

function statusSession(argumentsObj) {
  const result = runLauncher(["status", "--session", normalizeSession(argumentsObj)]);
  if (result.code !== 0) {
    return errorResult(result.stderr || result.stdout || `launcher exited with code ${result.code}`);
  }
  return textResult(result.stdout);
}

function endpointSession(argumentsObj) {
  const { endpoint, error } = getSessionEndpoint(argumentsObj);
  if (error) {
    return errorResult(error);
  }
  return textResult(endpoint);
}

async function listTabs(argumentsObj) {
  const { endpoint, error } = getSessionEndpoint(argumentsObj);
  if (error) {
    return errorResult(error);
  }
  try {
    const targets = await fetchPageTargets(endpoint);
    return textResult(JSON.stringify(targets, null, 2));
  } catch (err) {
    return errorResult(`Could not read tabs from ${normalizeEndpoint(endpoint)}: ${err.message || String(err)}`);
  }
}

async function closeTabs(argumentsObj) {
  const { endpoint, error } = getSessionEndpoint(argumentsObj);
  if (error) {
    return errorResult(error);
  }

  let targets;
  try {
    targets = await fetchPageTargets(endpoint);
  } catch (err) {
    return errorResult(`Could not read tabs from ${normalizeEndpoint(endpoint)}: ${err.message || String(err)}`);
  }

  const requestedIds = (argumentsObj?.target_ids || [])
    .map((value) => String(value).trim())
    .filter(Boolean);
  const urlContains = argumentsObj?.url_contains;
  const titleContains = argumentsObj?.title_contains;
  const allPages = Boolean(argumentsObj?.all_pages);

  let selected = [];
  let missingTargetIds = [];

  if (requestedIds.length > 0) {
    selected = targets.filter((target) => requestedIds.includes(String(target.id)));
    const selectedIds = new Set(selected.map((target) => String(target.id)));
    missingTargetIds = requestedIds.filter((targetId) => !selectedIds.has(targetId));
  } else if (urlContains || titleContains || allPages) {
    selected = targets;
    if (urlContains) {
      selected = selected.filter((target) => (target.url || "").includes(urlContains));
    }
    if (titleContains) {
      selected = selected.filter((target) => (target.title || "").includes(titleContains));
    }
  } else {
    return errorResult("Specify target_ids, url_contains/title_contains, or all_pages=true.");
  }

  if (selected.length === 0) {
    if (requestedIds.length > 0) {
      const summary = {
        session: normalizeSession(argumentsObj),
        endpoint: normalizeEndpoint(endpoint),
        closed: [],
        missing_target_ids: missingTargetIds,
        errors: [],
      };
      return errorResult(JSON.stringify(summary, null, 2));
    }
    return errorResult("No matching tabs found.");
  }

  const closed = [];
  const errors = [];
  for (const target of selected) {
    const targetId = String(target.id || "").trim();
    if (!targetId) {
      continue;
    }

    try {
      const responseText = await closePageTarget(endpoint, targetId);
      closed.push({
        id: targetId,
        title: target.title || "",
        url: target.url || "",
        response: responseText,
      });
    } catch (err) {
      const message = err.message || String(err);
      if (message.startsWith("HTTP 404")) {
        errors.push({ id: targetId, error: "Target not found" });
      } else {
        errors.push({ id: targetId, error: message });
      }
    }
  }

  const summary = {
    session: normalizeSession(argumentsObj),
    endpoint: normalizeEndpoint(endpoint),
    closed,
    missing_target_ids: missingTargetIds,
    errors,
  };

  if (errors.length > 0 && closed.length === 0) {
    return errorResult(JSON.stringify(summary, null, 2));
  }
  return textResult(JSON.stringify(summary, null, 2));
}

function stopSession(argumentsObj) {
  const result = runLauncher(["stop", "--session", normalizeSession(argumentsObj)]);
  if (result.code !== 0) {
    return errorResult(result.stderr || result.stdout || `launcher exited with code ${result.code}`);
  }
  return textResult(result.stdout || "stopped");
}

function listProfiles(argumentsObj) {
  const result = runLauncher(["list", "--session", normalizeSession(argumentsObj)]);
  if (result.code !== 0) {
    return errorResult(result.stderr || result.stdout || `launcher exited with code ${result.code}`);
  }
  return textResult(result.stdout);
}

const TOOLS = [
  {
    name: "start_session",
    description:
      "Start or reattach to a managed Clawbrowser session and return the local CDP endpoint. api_key is bootstrap-only when saved config.json reuse is unavailable.",
    inputSchema: {
      type: "object",
      properties: {
        session: { type: "string" },
        api_key: {
          type: "string",
          description:
            "Optional bootstrap-only API key from https://app.clawbrowser.ai. Omit it when the browser can reuse a saved config.json; provide it only for first-time bootstrap.",
        },
        image: { type: "string" },
        port: { type: "integer" },
        fingerprint: { type: "string" },
        regenerate: { type: "boolean" },
        country: { type: "string" },
        city: { type: "string" },
        connection_type: { type: "string" },
        verify_automation: { type: "boolean" },
      },
      additionalProperties: false,
    },
  },
  {
    name: "rotate_session",
    description:
      "Rotate the managed session with a fresh fingerprint/proxy-backed identity. api_key is bootstrap-only when saved config.json reuse is unavailable.",
    inputSchema: {
      type: "object",
      properties: {
        session: { type: "string" },
        api_key: {
          type: "string",
          description:
            "Optional bootstrap-only API key from https://app.clawbrowser.ai. Omit it when the browser can reuse a saved config.json; provide it only for first-time bootstrap.",
        },
        image: { type: "string" },
      },
      additionalProperties: false,
    },
  },
  {
    name: "status_session",
    description: "Show whether a managed Clawbrowser session is running.",
    inputSchema: {
      type: "object",
      properties: { session: { type: "string" } },
      additionalProperties: false,
    },
  },
  {
    name: "endpoint_session",
    description: "Return the CDP endpoint for a managed session.",
    inputSchema: {
      type: "object",
      properties: { session: { type: "string" } },
      additionalProperties: false,
    },
  },
  {
    name: "list_tabs",
    description: "List open page tabs in a managed Clawbrowser session.",
    inputSchema: {
      type: "object",
      properties: { session: { type: "string" } },
      additionalProperties: false,
    },
  },
  {
    name: "close_tabs",
    description:
      "Close one or more page tabs in a managed Clawbrowser session by target id, URL/title filter, or all pages.",
    inputSchema: {
      type: "object",
      properties: {
        session: { type: "string" },
        target_ids: {
          type: "array",
          items: { type: "string" },
          description: "Exact CDP target ids to close.",
        },
        url_contains: { type: "string" },
        title_contains: { type: "string" },
        all_pages: {
          type: "boolean",
          description: "Close every page tab in the session.",
        },
      },
      additionalProperties: false,
    },
  },
  {
    name: "stop_session",
    description: "Stop a managed Clawbrowser session.",
    inputSchema: {
      type: "object",
      properties: { session: { type: "string" } },
      additionalProperties: false,
    },
  },
  {
    name: "list_profiles",
    description: "List cached profiles in the managed session directory.",
    inputSchema: {
      type: "object",
      properties: { session: { type: "string" } },
      additionalProperties: false,
    },
  },
];

async function handleRequest(message) {
  const method = message?.method;
  const requestId = message?.id;

  if (method === "initialize") {
    // Trigger runtime bootstrap on extension startup so users do not need
    // separate pre-install steps before first tool usage.
    triggerRuntimeBootstrap();
    const protocol = message?.params?.protocolVersion || "2025-03-26";
    return {
      jsonrpc: "2.0",
      id: requestId,
      result: {
        protocolVersion: protocol,
        serverInfo: { name: SERVER_NAME, version: SERVER_VERSION },
        capabilities: { tools: { listChanged: false } },
      },
    };
  }

  if (method === "tools/list") {
    return {
      jsonrpc: "2.0",
      id: requestId,
      result: { tools: TOOLS },
    };
  }

  if (method === "tools/call") {
    const params = message?.params || {};
    const toolName = params.name;
    const argumentsObj = params.arguments || {};

    const handlers = {
      start_session: startSession,
      rotate_session: rotateSession,
      status_session: statusSession,
      endpoint_session: endpointSession,
      list_tabs: listTabs,
      close_tabs: closeTabs,
      stop_session: stopSession,
      list_profiles: listProfiles,
    };

    const handler = handlers[toolName];
    if (!handler) {
      return {
        jsonrpc: "2.0",
        id: requestId,
        error: { code: -32602, message: `Unknown tool: ${toolName}` },
      };
    }

    const result = await handler(argumentsObj);
    return { jsonrpc: "2.0", id: requestId, result };
  }

  if (requestId === undefined || requestId === null) {
    return null;
  }

  return {
    jsonrpc: "2.0",
    id: requestId,
    error: { code: -32601, message: `Unknown method: ${method}` },
  };
}

function writeMessage(message) {
  const payload = Buffer.from(JSON.stringify(message));
  const header = Buffer.from(`Content-Length: ${payload.length}\r\n\r\n`, "ascii");
  process.stdout.write(header);
  process.stdout.write(payload);
}

let inputBuffer = Buffer.alloc(0);
let queue = Promise.resolve();

function parseHeaders(rawHeaders) {
  const lines = rawHeaders.split("\r\n");
  const headers = {};
  for (const line of lines) {
    const idx = line.indexOf(":");
    if (idx === -1) {
      continue;
    }
    const key = line.slice(0, idx).trim().toLowerCase();
    const value = line.slice(idx + 1).trim();
    headers[key] = value;
  }
  return headers;
}

function processIncomingBuffer() {
  while (true) {
    const headerEnd = inputBuffer.indexOf("\r\n\r\n");
    if (headerEnd === -1) {
      return;
    }

    const rawHeaders = inputBuffer.slice(0, headerEnd).toString("utf8");
    const headers = parseHeaders(rawHeaders);
    const contentLength = Number.parseInt(headers["content-length"] || "0", 10);
    if (!Number.isFinite(contentLength) || contentLength <= 0) {
      inputBuffer = inputBuffer.slice(headerEnd + 4);
      continue;
    }

    const bodyStart = headerEnd + 4;
    const messageEnd = bodyStart + contentLength;
    if (inputBuffer.length < messageEnd) {
      return;
    }

    const rawBody = inputBuffer.slice(bodyStart, messageEnd).toString("utf8");
    inputBuffer = inputBuffer.slice(messageEnd);

    let message;
    try {
      message = JSON.parse(rawBody);
    } catch (_err) {
      continue;
    }

    queue = queue
      .then(async () => {
        const response = await handleRequest(message);
        if (response) {
          writeMessage(response);
        }
      })
      .catch((err) => {
        log(`ERROR: ${err.message || String(err)}`);
        if (message && message.id !== undefined && message.id !== null) {
          writeMessage({
            jsonrpc: "2.0",
            id: message.id,
            error: { code: -32603, message: err.message || String(err) },
          });
        }
      });
  }
}

function main() {
  process.stdin.on("data", (chunk) => {
    inputBuffer = Buffer.concat([inputBuffer, chunk]);
    processIncomingBuffer();
  });
}

main();
