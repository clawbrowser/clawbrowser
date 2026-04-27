"""Tool schemas -- what the LLM sees for Clawbrowser browser tools."""

CLAWBROWSER_START = {
    "name": "clawbrowser_start",
    "description": (
        "Start or reattach a managed Clawbrowser session and return the "
        "local CDP endpoint. Use this as the supported launch path for "
        "agent tasks, then use CDP for navigation, clicking, typing, "
        "scraping, screenshots, DOM inspection, and JS evaluation. After "
        "start, reattach, restart, or rotate, call clawbrowser_endpoint to "
        "get the current temporary endpoint. For fingerprint/proxy/identity "
        "proof, open clawbrowser://verify/ inside the managed session and "
        "inspect it through CDP. api_key is bootstrap-only when saved "
        "config.json reuse is unavailable. If you need to create "
        "config.json manually, resolve the absolute path first and do not "
        "pass unresolved shell-expression paths to file/write tools. They "
        "may create literal workspace paths instead of the real config "
        "file."
    ),
    "parameters": {
        "type": "object",
        "properties": {
            "session": {
                "type": "string",
                "description": (
                    "Managed session name. Use distinct names for separate "
                    "identities or simultaneous profiles."
                ),
            },
            "url": {
                "type": "string",
                "description": (
                    "Optional URL to open after the session starts, including "
                    "clawbrowser://verify/ or clawbrowser://auth when needed."
                ),
            },
            "api_key": {
                "type": "string",
                "description": (
                    "Bootstrap-only API key from https://app.clawbrowser.ai. "
                    "Omit it when the browser can reuse saved config.json. If "
                    "you need to create config.json manually, resolve the "
                    "absolute path first and do not pass unresolved shell-"
                    "expression paths to file/write tools. They may create "
                    "literal workspace paths instead of the real config file."
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
                "description": "Open clawbrowser://verify/ when no URL is supplied.",
            },
            "fingerprint": {
                "type": ["boolean", "string"],
                "description": "Optional fingerprint mode override. Omit for the launcher default; managed sessions are expected to run in fingerprint mode. Values like none/off/disabled are rejected.",
            },
            "regenerate": {
                "type": "boolean",
                "description": "Pass --regenerate for a fresh identity. Prefer clawbrowser rotate for the public fresh-identity path.",
            },
            "verify_automation": {
                "type": "boolean",
                "description": "Pass --verify-automation to the browser runtime when you need automation verification.",
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
        "Return the current temporary CDP endpoint for a managed session. "
        "Do not persist it; call clawbrowser_endpoint again after start, "
        "reattach, restart, or rotate. Use this endpoint for page automation; "
        "do not use it as a lifecycle command."
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
        "Restart the named managed session with a fresh identity. This is "
        "the official fresh-identity path; use it instead of UI controls or "
        "manual browser launches. After rotation, call clawbrowser_endpoint "
        "again to refresh the current temporary endpoint, and use "
        "clawbrowser://verify/ for fingerprint/proxy proof."
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
                "description": "Optional URL to open after rotation, including clawbrowser://verify/.",
            },
            "api_key": {
                "type": "string",
                "description": (
                    "Bootstrap-only API key from https://app.clawbrowser.ai. "
                    "Omit it when the browser can reuse saved config.json. If "
                    "you need to create config.json manually, resolve the "
                    "absolute path first and do not pass unresolved shell-"
                    "expression paths to file/write tools. They may create "
                    "literal workspace paths instead of the real config file."
                ),
            },
            "image": {
                "type": "string",
                "description": "Optional container image override.",
            },
            "verify": {
                "type": "boolean",
                "description": "Open clawbrowser://verify/ when no URL is supplied.",
            },
            "fingerprint": {
                "type": ["boolean", "string"],
                "description": "Optional fingerprint mode override. Omit for the launcher default; managed sessions are expected to run in fingerprint mode. Values like none/off/disabled are rejected.",
            },
            "regenerate": {
                "type": "boolean",
                "description": "Pass --regenerate for a fresh identity. Prefer clawbrowser rotate for the public fresh-identity path.",
            },
            "verify_automation": {
                "type": "boolean",
                "description": "Pass --verify-automation to the browser runtime when you need automation verification.",
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
        "Open a URL in a running managed session through the current CDP "
        "endpoint. Use this after obtaining the endpoint from "
        "clawbrowser_endpoint."
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
    "description": "List open page tabs in a managed session so you can inspect or clean up browser pages.",
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
        "Close selected page tabs in a managed session by target id, URL/title "
        "filter, or all_pages. Use this for about:blank and finished-tab cleanup "
        "without stopping the session."
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
        "Stop a managed session. Use only when the user explicitly asks to "
        "close the session or when performing explicit cleanup."
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
        "Check whether a managed session is running and report its CDP "
        "endpoint and backend."
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
