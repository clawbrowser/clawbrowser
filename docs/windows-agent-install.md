# Windows Agent Install

Human users can double-click `setup.exe` and get the interactive Clawbrowser
Setup window with Install, Cancel, install status, and Close controls.

AI agents on Windows should run the Clawbrowser installer with the normal
silent installer switch:

```powershell
.\setup.exe /silent
```

This bypasses the interactive window and installs Clawbrowser without launching
browser UI after setup.
