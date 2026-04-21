# browser_guard

`browser_guard` is a Windows C utility that suspends supported browsers when they are minimized or no longer in the foreground, then resumes them the moment they become active again or start outputting audio. The project is aimed at reducing background CPU churn and shrinking resident memory pressure without permanently killing the browser.

## What the project does now

- Detects supported browser processes owned by the current user in the current logon session.
- Keeps the foreground browser alive.
- Keeps browsers alive when they have an active Windows audio session.
- Suspends background browsers with `NtSuspendProcess`.
- Shows a top-left reminder while a browser is suspended.
- Lets you click the reminder or click the suspended browser window to wake it back up.
- Waits briefly before suspending a newly backgrounded browser so normal app switching does not feel sticky.
- Keeps a manually resumed browser awake for a short grace period to avoid immediate re-locks.
- Optionally trims working sets after suspension and re-trims them on a fixed interval.
- Optionally lowers memory priority and enables Windows power throttling while the browser is suspended.
- Restores suspended browsers automatically when they return to the foreground or when the tool exits.

Supported browser families:

- `chrome.exe`
- `msedge.exe`
- `firefox.exe`
- `brave.exe`
- `opera.exe`
- `vivaldi.exe`

## Memory behavior

This project can reduce the browser's resident memory footprint very aggressively, but Windows memory accounting matters here:

- `Working set` is the amount of memory currently resident in RAM.
- `Private bytes` is committed private memory. It often falls much less than working set, because suspension and trimming do not force the browser to fully decommit its heaps.

That means `browser_guard` is very good at giving RAM back to the system cache and other apps, but it is not the same as closing the browser. The benchmark included in this repository makes that distinction visible.

## Architecture

The codebase is split so each layer has a narrow job:

- [src/main.c](C:\Users\user\Documents\Codex\2026-04-21-c-code-github-repo\src\main.c): CLI entry point
- [src/app_config.c](C:\Users\user\Documents\Codex\2026-04-21-c-code-github-repo\src\app_config.c): argument parsing and defaults
- [src/browser_guard.c](C:\Users\user\Documents\Codex\2026-04-21-c-code-github-repo\src\browser_guard.c): orchestration loop and lifecycle tracking
- [src/control_main.c](C:\Users\user\Documents\Codex\2026-04-21-c-code-github-repo\src\control_main.c): native desktop toggle and startup launcher
- [src/process_control.c](C:\Users\user\Documents\Codex\2026-04-21-c-code-github-repo\src\process_control.c): process discovery, ownership checks, audio detection, suspension, and memory policy

Public headers live under [include](C:\Users\user\Documents\Codex\2026-04-21-c-code-github-repo\include).

## Security and stability posture

This tool touches live processes, so the guardrails matter as much as the feature:

- Only processes in an explicit browser whitelist are considered.
- Only processes owned by the same Windows user are managed.
- Only processes in the same session are managed.
- Handles, COM interfaces, and temporary token buffers are always released on the same control path that acquired them.
- The runtime uses bounded arrays for tracked groups and tracked processes to avoid accidental heap growth.
- Suspension is reversible. On shutdown, tracked processes are resumed automatically.

## Memory-leak posture

The program is intentionally conservative with allocation:

- The main runtime loop uses stack storage for browser groups and tracked process state.
- Token inspection uses bounded stack buffers first and falls back to `HeapAlloc` only when Windows reports a larger token payload.
- Every `OpenProcess`, `OpenProcessToken`, COM object acquisition, and heap allocation has a matching release path.

This does not prove the absence of bugs, but it narrows the number of places where leaks can happen and makes review easier.

## Build

### MSVC

```powershell
cmake -S . -B build
cmake --build build --config Release
```

This produces both:

- `build\Release\browser_guard.exe`
- `build\Release\browser_guard_control.exe`

### MinGW

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

## Run

Minimal run:

```powershell
.\build\Release\browser_guard.exe
```

On Windows, `browser_guard.exe` now starts as a background GUI process, so double-clicking it does not open a console window.

Recommended aggressive mode:

```powershell
.\build\Release\browser_guard.exe --aggressive-memory --trim-interval-ms 3000
```

When a browser is paused, a small top-left overlay appears. Clicking that overlay resumes suspended browsers immediately. You can also click the paused browser window itself; `browser_guard` will resume it and try to bring it back to the foreground.

Available options:

- `--interval-ms N`: main polling interval, default `1000`
- `--trim-working-set`: trim browser working sets after suspension
- `--trim-interval-ms N`: re-trim suspended browsers every `N` milliseconds
- `--background-grace-ms N`: wait this long before suspending a background browser
- `--manual-resume-grace-ms N`: keep a manually resumed browser awake for this long
- `--lower-memory-priority`: lower process memory priority while suspended
- `--eco-qos`: apply Windows power throttling while suspended
- `--aggressive-memory`: shortcut for trim + low memory priority + power throttling
- `--verbose`: print state transitions and memory totals every loop

## Install on Windows

To install a startup shortcut for the current user:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\install-startup.ps1
```

This copies `browser_guard.exe` into `%LOCALAPPDATA%\browser_guard` and creates a shortcut in the Windows Startup folder so it launches at sign-in.

The installer also copies `browser_guard_control.exe` into the same directory and creates a desktop shortcut named `browser_guard Toggle`. That shortcut is a native Windows controller:

- click it once to turn `browser_guard` off
- click it again later to turn `browser_guard` back on
- each toggle shows a small native confirmation popup near the bottom-right corner
- startup launches now go through `browser_guard_control.exe --launch`, which respects the disabled flag and avoids re-enabling the guard after you intentionally turned it off

To remove that installation:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\uninstall-startup.ps1
```

## Benchmark and comparison

This repository includes a repeatable benchmark script:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\compare-memory.ps1
```

Outputs:

- [docs/memory-benchmark.csv](C:\Users\user\Documents\Codex\2026-04-21-c-code-github-repo\docs\memory-benchmark.csv)
- [docs/memory-benchmark.md](C:\Users\user\Documents\Codex\2026-04-21-c-code-github-repo\docs\memory-benchmark.md)

Recommended benchmark condition:

1. Open the browser normally.
2. Put it on the side monitor or leave it minimized.
3. Focus another app during the guarded phase.

### Latest local sample on this Windows machine

The benchmark script was executed in this workspace on `2026-04-21` with `browser_guard.exe --aggressive-memory --trim-interval-ms 3000`.

| Phase | Avg process count | Avg working set (MB) | Avg private bytes (MB) |
| --- | ---: | ---: | ---: |
| Baseline | 39 | 8.05 | 6991.09 |
| Guarded | 39 | 0.31 | 6991.11 |

Result:

- Working set dropped by `7.74 MB` or `96.15%`.
- Private bytes increased by `0.02 MB`, which is effectively unchanged.

Interpretation:

- This specific sample was taken after the browser had already spent time in the background, so the baseline working set was already low.
- The project still reduced the remaining resident footprint by `96.15%` in that run.
- The project does not currently force large private-byte reductions, because that would require the browser itself to discard heaps or unload content.
- Cold or heavily used browser sessions can show much larger working-set drops than this warmed-up sample.
- In practice, this still helps when a game or other heavy foreground app needs RAM immediately, because Windows can repurpose the trimmed pages much more easily.

## Limitations

- Muted video playback can still be suspended, because Windows does not expose a universal cross-browser "video is playing" signal.
- Some browser helper processes can reject management if Windows denies access.
- Private-byte reductions are expected to be smaller than working-set reductions.

## Repository

GitHub repository: [Sakuya4/browser-guard](https://github.com/Sakuya4/browser-guard)
