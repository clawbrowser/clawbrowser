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
        },
        "required": ["session"],
    },
}

CLAWBROWSER_STOP = {
    "name": "clawbrowser_stop",
    "description": (
        "Stop a Clawbrowser browser session and clean up its resources. "
        "Always stop sessions when the browser task is complete to free "
        "resources. Close individual tabs first if you only need to clean "
        "up part of a session."
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
