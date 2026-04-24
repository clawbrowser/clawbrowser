"""Tool schemas -- what the LLM sees for Clawbrowser browser tools."""

CLAWBROWSER_START = {
    "name": "clawbrowser_start",
    "description": (
        "Start a Clawbrowser browser session with an isolated identity. "
        "Returns a live CDP (Chrome DevTools Protocol) endpoint URL. "
        "Optionally opens a URL in the new session. Use this to begin any "
        "browser or web task without touching the user's personal browser. "
        "In auto mode, falls back from native app to Docker container if "
        "native startup fails."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "session": {
                "type": "string",
                "description": (
                    "Session name to create or reattach to. Use distinct "
                    "names for separate tasks (e.g., 'research', 'scrape')."
                ),
            },
            "url": {
                "type": "string",
                "description": (
                    "Optional URL to open immediately after the session "
                    "starts. Omit to start with a blank page."
                ),
            },
            "api_key": {
                "type": "string",
                "description": (
                    "Optional bootstrap-only API key from https://app.clawbrowser.ai. "
                    "Omit when Clawbrowser can reuse saved config.json."
                ),
            },
            "image": {
                "type": "string",
                "description": "Optional container image override.",
            },
            "port": {
                "type": "integer",
                "description": "Optional host CDP port.",
            },
            "verify": {
                "type": "boolean",
                "description": "Open clawbrowser://verify when no URL is supplied.",
            },
            "fingerprint": {
                "type": ["boolean", "string"],
                "description": "Optional fingerprint mode override. Omit for launcher default; true passes --fingerprint.",
            },
            "regenerate": {
                "type": "boolean",
                "description": "Pass --regenerate for a fresh profile.",
            },
            "verify_automation": {
                "type": "boolean",
                "description": "Pass --verify-automation to the browser runtime.",
            },
            "country": {
                "type": "string",
                "description": "Optional country hint for the managed identity.",
            },
            "city": {
                "type": "string",
                "description": "Optional city hint for the managed identity.",
            },
            "connection_type": {
                "type": "string",
                "description": "Optional connection type hint for the managed identity.",
            },
        },
        "required": ["session"],
    },
}

CLAWBROWSER_ENDPOINT = {
    "name": "clawbrowser_endpoint",
    "description": (
        "Get the live CDP endpoint URL for an existing Clawbrowser session. "
        "Use this to retrieve the WebSocket debugger URL for connecting "
        "browser automation tools (Playwright, Puppeteer, etc.) to the "
        "running session."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "session": {
                "type": "string",
                "description": "Name of the session to query.",
            },
        },
        "required": ["session"],
    },
}

CLAWBROWSER_ROTATE = {
    "name": "clawbrowser_rotate",
    "description": (
        "Rotate the browser identity for an existing Clawbrowser session. "
        "Generates a fresh fingerprint, proxy assignment, and browser "
        "profile. Use this when you need a new identity without stopping "
        "and restarting the session, such as when switching between "
        "accounts or avoiding rate limits."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "session": {
                "type": "string",
                "description": "Name of the session to rotate.",
            },
            "url": {
                "type": "string",
                "description": "Optional URL to open after rotation, such as clawbrowser://verify.",
            },
            "api_key": {
                "type": "string",
                "description": (
                    "Optional bootstrap-only API key from https://app.clawbrowser.ai. "
                    "Omit when Clawbrowser can reuse saved config.json."
                ),
            },
            "image": {
                "type": "string",
                "description": "Optional container image override.",
            },
            "verify": {
                "type": "boolean",
                "description": "Open clawbrowser://verify when no URL is supplied.",
            },
            "fingerprint": {
                "type": ["boolean", "string"],
                "description": "Optional fingerprint mode override. Omit for launcher default; true passes --fingerprint.",
            },
            "regenerate": {
                "type": "boolean",
                "description": "Pass --regenerate for a fresh profile.",
            },
            "verify_automation": {
                "type": "boolean",
                "description": "Pass --verify-automation to the browser runtime.",
            },
            "country": {
                "type": "string",
                "description": "Optional country hint for the managed identity.",
            },
            "city": {
                "type": "string",
                "description": "Optional city hint for the managed identity.",
            },
            "connection_type": {
                "type": "string",
                "description": "Optional connection type hint for the managed identity.",
            },
        },
        "required": ["session"],
    },
}

CLAWBROWSER_OPEN_URL = {
    "name": "clawbrowser_open_url",
    "description": (
        "Open a URL in an existing managed Clawbrowser session using the "
        "session's CDP endpoint. Use this for normal URLs and "
        "clawbrowser://auth or clawbrowser://verify."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "session": {
                "type": "string",
                "description": "Name of the session to use.",
            },
            "url": {
                "type": "string",
                "description": "URL to open in Clawbrowser.",
            },
        },
        "required": ["session", "url"],
    },
}

CLAWBROWSER_LIST_TABS = {
    "name": "clawbrowser_list_tabs",
    "description": "List open page tabs in a managed Clawbrowser session.",
    "parameters": {
        "type": "object",
        "properties": {
            "session": {
                "type": "string",
                "description": "Name of the session to inspect.",
            },
        },
        "required": ["session"],
    },
}

CLAWBROWSER_CLOSE_TABS = {
    "name": "clawbrowser_close_tabs",
    "description": (
        "Close tabs in a managed Clawbrowser session by target id, URL/title "
        "filter, or all_pages. Use this to close about:blank and finished "
        "task tabs without stopping the session."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "session": {
                "type": "string",
                "description": "Name of the session to clean up.",
            },
            "target_ids": {
                "type": "array",
                "items": {"type": "string"},
                "description": "Exact CDP target ids to close.",
            },
            "url_contains": {
                "type": "string",
                "description": "Close tabs whose URL contains this text.",
            },
            "title_contains": {
                "type": "string",
                "description": "Close tabs whose title contains this text.",
            },
            "all_pages": {
                "type": "boolean",
                "description": "Close every page tab in the session.",
            },
        },
        "required": ["session"],
    },
}

CLAWBROWSER_STOP = {
    "name": "clawbrowser_stop",
    "description": (
        "Stop a Clawbrowser browser session and clean up its resources. "
        "Use this only when the user explicitly asks to close the session "
        "or when performing explicit cleanup. For normal task cleanup, close "
        "blank or no-longer-needed tabs instead."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "session": {
                "type": "string",
                "description": "Name of the session to stop.",
            },
        },
        "required": ["session"],
    },
}

CLAWBROWSER_STATUS = {
    "name": "clawbrowser_status",
    "description": (
        "Check the status of a Clawbrowser browser session. Returns "
        "whether the session is running, its CDP endpoint, backend type "
        "(native or container), and other session metadata."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "session": {
                "type": "string",
                "description": "Name of the session to check.",
            },
        },
        "required": ["session"],
    },
}
