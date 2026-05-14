# Windows Agent Install

Human users can double-click `setup.exe` and get the interactive Clawbrowser
Setup window with Install, Cancel, install status, and Close controls.

AI agents on Windows should run the Clawbrowser installer with the agent flag:

```powershell
.\setup.exe --clawbrowser-agent-install
```

This bypasses the interactive window and installs Clawbrowser without launching
browser UI after setup. Internally the flag maps to Chromium installer
preferences that disable both direct post-install launch and update-service
post-install launch registration.
