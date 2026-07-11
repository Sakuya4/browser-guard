browser_guard for Windows

Files in this package:
- browser_guard.exe
- browser_guard_control.exe
- browser_guard_recovery.exe
- install-startup.ps1
- uninstall-startup.ps1
- Install Startup.bat
- Uninstall Startup.bat
- Run Portable.bat

Quick start:
1. Double-click "Run Portable.bat" to try it immediately.
2. Double-click "Install Startup.bat" if you want browser_guard to start automatically when you sign in.
3. After installation, use the desktop shortcut "browser_guard Toggle" to turn it off or on.
4. Double-click "Uninstall Startup.bat" to remove the startup installation later.

Behavior:
- browser_guard.exe runs in the background and does not open a console window.
- browser_guard_recovery.exe independently resumes validated processes if the guard exits unexpectedly.
- Toggling browser_guard on or off shows a small native confirmation popup near the bottom-right corner.
- The default policy only suspends a browser family when all of its visible windows are minimized.
- Background suspension waits about one minute by default before it triggers.
- When a supported browser is paused, a small overlay appears in the top-left corner.
- Click that overlay or click the paused browser window to resume it.

Safety note:
- Keep all three executable files together.
- Process suspension is experimental and cannot detect downloads, muted video, meetings, or unsaved forms.
- Use aggressive suspension only after reading the project Safety Model.

Supported browsers:
- Chrome
- Microsoft Edge
- Firefox
- Brave
- Opera
- Vivaldi
