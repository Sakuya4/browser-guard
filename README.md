# browser_guard

`browser_guard` is a Windows C utility that freezes supported browsers when they are not the foreground app, then resumes them as soon as they become active again or start playing audio.

## Why this exists

Modern browsers keep multiple processes alive even when they are sitting on a side monitor or minimized. This project takes an aggressive but reversible approach:

- If a supported browser is not the foreground app, its processes are suspended.
- If that browser is actively outputting audio, it is kept alive so music or video playback can continue.
- An optional working-set trim can be enabled to push memory usage down further after suspension.

## Current behavior

Supported browser families:

- `chrome.exe`
- `msedge.exe`
- `firefox.exe`
- `brave.exe`
- `opera.exe`
- `vivaldi.exe`

Suspension rules:

- Foreground browser: keep running
- Background browser with active audio session: keep running
- Background browser without active audio session: suspend

## Important limitation

This version detects active audio sessions, not "video frames". A muted video can still be suspended because Windows does not expose a simple, reliable "video is playing" signal across all browsers.

## Build

### MSVC

```powershell
cmake -S . -B build
cmake --build build --config Release
```

### MinGW

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## Usage

```powershell
.\build\browser_guard.exe --verbose
```

Optional flags:

- `--interval-ms <n>`: polling interval, default `1000`
- `--trim-working-set`: trim working set after suspension
- `--verbose`: print suspend/resume decisions

Press `Ctrl+C` to exit. Any suspended browser processes tracked by the tool are resumed during shutdown.

## Notes

- You may need to run with the same user account that owns the browser processes.
- This is an MVP intended for experimentation. Test with a non-critical browsing session first.
