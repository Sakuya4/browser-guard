# browser_guard

`browser_guard` is an experimental Windows resource-management utility written in C11. It reduces background browser CPU activity and resident memory pressure while using a fail-closed recovery design: if safe recovery is unavailable, the guard refuses to suspend browser processes.

The project is designed as a practical Windows systems-programming case study, not as a claim that process-level suspension understands browser tabs.

## Why this project exists

Browsers can retain a large resident footprint while a game, compiler, EDA tool, or local model needs memory immediately. `browser_guard` applies reversible process policies to supported browsers owned by the current Windows user and logon session.

The default policy is deliberately conservative:

- only browser families whose visible windows are all minimized are eligible for suspension;
- foreground and active-audio browser families remain running;
- newly backgrounded browsers receive a grace period;
- aggressive background suspension requires explicit opt-in;
- every suspend is journaled before it occurs;
- an independent broker resumes journaled processes if the guard exits unexpectedly.

## Current architecture

The build produces three native executables:

| Component | Responsibility |
| --- | --- |
| `browser_guard.exe` | Discovers browsers, evaluates policy, applies process state, and owns the overlay. |
| `browser_guard_control.exe` | Starts the guard or requests authenticated, graceful shutdown. It does not force-terminate the guard. |
| `browser_guard_recovery.exe` | Waits independently and resumes validated journal records after an unexpected guard exit. |

See [Architecture](docs/architecture.md), [Safety Model](docs/safety.md), and the [v1 product specification](docs/specs/productization-v1.md).

## Supported browsers

- Google Chrome (`chrome.exe`)
- Microsoft Edge (`msedge.exe`)
- Mozilla Firefox (`firefox.exe`)
- Brave (`brave.exe`)
- Opera (`opera.exe`)
- Vivaldi (`vivaldi.exe`)

Only processes belonging to the current Windows user and current logon session are eligible.

## Quick start

Requirements:

- Windows 10 or Windows 11 x64
- Visual Studio 2022 Build Tools with the Desktop development with C++ workload
- CMake 3.20 or newer

Configure, build, and test:

```powershell
cmake -S . -B build -A x64 -DBG_WARNINGS_AS_ERRORS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Run the conservative policy:

```powershell
.\build\Release\browser_guard.exe
```

Keep `browser_guard.exe`, `browser_guard_control.exe`, and `browser_guard_recovery.exe` in the same directory.

## Install for the current user

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\install-startup.ps1
```

The installer copies all three executables to `%LOCALAPPDATA%\browser_guard`, creates a per-user Startup shortcut, and creates a desktop Toggle shortcut. Its default arguments explicitly use `--minimized-only`; `--aggressive-suspend` is never enabled implicitly.

Upgrade an existing installation:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\install-startup.ps1 -Overwrite
```

Upgrade and uninstall request graceful shutdown first. If the guard or recovery broker does not exit safely, the script refuses to replace or delete the running components.

Uninstall:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\uninstall-startup.ps1
```

## Command-line options

| Option | Meaning |
| --- | --- |
| `--interval-ms N` | Main polling interval, default `1000`. |
| `--background-grace-ms N` | Delay before an eligible browser may be suspended, default `60000`. |
| `--manual-resume-grace-ms N` | Hold time after manual resume. |
| `--minimized-only` | Suspend only when every visible window in the browser family is minimized; this is the default. |
| `--aggressive-suspend` | Allow all background browser families to be suspended. Explicit opt-in only. |
| `--trim-working-set` | Ask Windows to trim resident pages after suspension. |
| `--trim-interval-ms N` | Re-trim interval when working-set trimming is enabled. |
| `--lower-memory-priority` | Lower memory priority while managed in the background. |
| `--eco-qos` | Request Windows power throttling. |
| `--aggressive-memory` | Enable trim, lower memory priority, and EcoQoS together; it does not enable aggressive suspension. |
| `--heartbeat-interval-ms N` | Interval for the legacy suspended-window probe. |
| `--window-probe-timeout-ms N` | Timeout used by that window probe. |
| `--verbose` | Print lifecycle, protection, and memory decisions. |

## What the memory numbers mean

- **Working set** is memory currently resident in physical RAM.
- **Private bytes** is committed private memory owned by the browser.

Trimming a working set can make resident pages available to other workloads without making the browser decommit its private heaps. A lower working set is therefore not the same as closing tabs or freeing the same amount of private memory. Restoring trimmed pages can also create page faults and visible latency.

The repository intentionally does not publish the earlier single-run `96.15%` figure as product evidence. The current benchmark uses repeated counterbalanced trials and reports costs alongside benefits:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\compare-memory.ps1
```

See the [benchmark report placeholder and methodology](docs/memory-benchmark.md).

## Safety boundaries

The project protects against several process-management failures:

- controller shutdown uses a per-user, per-session named event with an explicit access-control list;
- the controller verifies full executable path, Windows user, and logon session;
- recovery records contain PID, process creation time, session, and executable path to defend against PID reuse;
- a suspend is refused if the recovery record cannot be persisted or the broker is unavailable;
- mixed minimized/restored browser windows are covered by regression tests;
- integration tests use dedicated fixture processes, never the developer's browser.

It cannot reliably detect browser-internal work such as muted video, downloads, uploads, WebRTC, microphone capture, unsaved forms, service workers, WebUSB, or long-running JavaScript. Process-level suspend remains experimental and uses the undocumented `NtSuspendProcess`/`NtResumeProcess` APIs.

Read [Safety Model](docs/safety.md) before enabling aggressive suspension.

## Tests and CI

CTest covers:

- mixed multi-window suspension policy;
- per-user/session shutdown event behavior;
- path, user, and session process identity;
- graceful controller shutdown;
- recovery-journal persistence and PID reuse protection;
- broker recovery after simulated owner crash.

GitHub Actions builds Debug and Release on `windows-2022` with MSVC `/W4 /WX`, then runs the complete CTest suite.

## Roadmap

- replace the legacy periodic resume/probe heartbeat with handle-based liveness monitoring;
- add explainable policy reason telemetry locally, without network collection;
- restore background memory/power policy metadata during broker recovery;
- expand Windows 10/11 and browser-version manual test matrices;
- investigate an optional browser-extension/native-messaging layer for tab-level semantics;
- add a tray UI only after the safety core remains stable.

## Project documentation

- [Architecture](docs/architecture.md)
- [Safety Model](docs/safety.md)
- [English interview demo](docs/interview-demo.md)
- [繁體中文面試講稿](docs/interview-demo.zh-TW.md)
- [Contributing](CONTRIBUTING.md)
- [Security Policy](SECURITY.md)
- [Changelog](CHANGELOG.md)
- [Architecture Decision Records](docs/decisions/)

## License

MIT. See [LICENSE](LICENSE).
