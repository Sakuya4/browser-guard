# Spec: browser_guard Productization v1

## Status

Approved by the project owner on 2026-07-12.

## Objective

Turn `browser_guard` from a Windows API prototype into a trustworthy, project-ready open-source product that demonstrates practical C engineering.

The primary user is a Windows 10/11 user who temporarily needs more resident memory for games, builds, EDA workloads, or local model inference. The product must prefer preserving browser work over reclaiming memory.

Product promise:

> Reduce background browser resource pressure without leaving the browser frozen or hiding the trade-offs from the user.

The v1 portfolio release should demonstrate:

- defensive Windows process management;
- explicit lifecycle and recovery protocols;
- testable policy logic separated from Windows side effects;
- repeatable measurements that include costs as well as benefits;
- automated Windows builds and tests;
- documentation suitable for users, contributors, and project discussion.

## Scope

### Milestone 1: Safe control plane

- Replace the controller's unconditional `TerminateProcess` shutdown with an authenticated, per-user/per-session named shutdown event.
- Make the guard resume every tracked process before normal exit.
- Make the controller wait for graceful exit and report a failure if the timeout expires; forced termination must not be the normal path.
- Verify a discovered guard process by executable path, current user, and logon session before controlling it.
- Change startup installation defaults to the conservative minimized-only policy.
- Ensure installer upgrades request graceful shutdown rather than using `Stop-Process -Force` as the normal path.

### Milestone 2: Testable and conservative policy

- Extract suspension eligibility into a side-effect-free policy module.
- Model window state using counts/flags that distinguish visible restored windows from minimized windows.
- A browser family is ineligible for suspension if any owned top-level window is visible and not minimized under the minimized-only policy.
- Keep foreground and active-audio protection.
- Return an explainable decision reason such as foreground, audio, visible window, grace period, manual resume, or eligible.
- Treat capacity exhaustion and inspection failures conservatively: do not suspend what cannot be classified safely.

### Milestone 3: Crash recovery

- Add a small native recovery broker executable.
- Before suspending a process, the guard records the process identity needed for recovery with the broker.
- A process is removed from the recovery set only after a successful resume or confirmed process exit.
- The broker monitors guard liveness and resumes registered processes if the guard exits unexpectedly.
- Recovery records must defend against PID reuse by including process creation time and executable identity.
- Recovery is scoped to the current Windows user and logon session.

### Milestone 4: Evidence and open-source presentation

- Add unit tests for configuration parsing, policy decisions, time/deadline wraparound, capacity limits, and recovery-record validation.
- Add Windows integration tests for graceful controller shutdown and broker recovery where they can run deterministically.
- Add GitHub Actions for MSVC build, CTest, and warning checks on Windows.
- Redesign the benchmark to run repeated alternating baseline/guarded trials and report sample count, mean, spread, working set, private bytes, CPU time, page faults, and resume latency.
- Remove misleading headline percentages from the README unless paired with absolute values and test conditions.
- Add architecture, safety model, limitations, contributing guide, changelog, and ADRs.
- Add a concise project demo script covering the problem, design, failure modes, trade-offs, tests, and measured results.

## Explicit Non-goals for v1

- Browser extension or per-tab control.
- Detecting every form, download, WebRTC, WebUSB, or browser-internal activity state from the native process layer.
- Claiming that working-set trimming deallocates browser private memory.
- Kernel drivers, system-wide services, telemetry, cloud accounts, or remote control.
- Code-signing and automatic updates.
- A full settings dashboard or polished tray application.

These are roadmap items only after the native safety foundation is proven.

## Tech Stack

- Language: ISO C11.
- Platform API: Win32, Tool Help, PSAPI, Core Audio, process power/memory APIs.
- Build: CMake 3.20 or newer.
- Primary compiler: MSVC from Visual Studio 2022 on Windows x64.
- Tests: CTest with small native C test executables and no third-party test framework initially.
- Automation: GitHub Actions Windows runners.
- Scripts: PowerShell for installation and benchmarking.

Undocumented `NtSuspendProcess` and `NtResumeProcess` remain an explicitly labeled experimental mechanism. Conservative behavior and crash recovery are required around their use.

## Commands

Configure:

```powershell
cmake -S . -B build -A x64
```

Build:

```powershell
cmake --build build --config Release
```

Test:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Install conservatively for the current user:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\install-startup.ps1
```

Run benchmark after building:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\compare-memory.ps1
```

## Project Structure

```text
include/                 Public module interfaces
src/                     Guard, controller, recovery broker, and policy implementation
tests/                   Native unit and Windows integration tests
tools/                   Installation, uninstallation, and benchmark scripts
docs/specs/              Product and feature specifications
docs/decisions/          Architecture decision records
docs/architecture.md     Components, protocols, and trust boundaries
docs/safety.md           Safety guarantees, limitations, and recovery behavior
docs/project-demo.md   Repeatable project presentation and demonstration
.github/workflows/       Windows CI quality gates
```

## Code Style

- Use `snake_case` for functions and local variables, `PascalCase` for structs/enums, and `UPPER_SNAKE_CASE` for constants/macros.
- Keep Windows handles and COM interfaces on a control path with a visible matching release.
- Prefer small pure functions for policy; isolate Win32 calls in adapter modules.
- Return explicit success/failure values and default to no suspension when information is incomplete.
- Compile at a high warning level and treat project warnings as errors in CI.

Example:

```c
SuspendDecision evaluate_suspend_policy(const BrowserState *state, const PolicyConfig *config) {
    if (!state->inspection_complete) {
        return (SuspendDecision){false, SUSPEND_REASON_INSPECTION_FAILED};
    }
    if (state->has_foreground_window) {
        return (SuspendDecision){false, SUSPEND_REASON_FOREGROUND};
    }
    if (state->has_audio) {
        return (SuspendDecision){false, SUSPEND_REASON_AUDIO};
    }
    if (config->minimized_only && state->visible_restored_window_count > 0) {
        return (SuspendDecision){false, SUSPEND_REASON_VISIBLE_WINDOW};
    }
    return (SuspendDecision){true, SUSPEND_REASON_ELIGIBLE};
}
```

## Testing Strategy

### Small unit tests

- Argument/default parsing, including invalid numeric boundaries.
- Suspension policy truth table, especially mixed minimized/restored windows.
- Foreground, audio, grace-period, and manual-resume precedence.
- Tick-count wraparound-safe deadline checks.
- Fixed-capacity behavior and conservative failure decisions.
- Recovery identity validation, including simulated PID reuse.

### Medium integration tests

- Start a controllable fixture process, request guard shutdown through the controller, and assert graceful guard exit.
- Simulate unexpected guard termination with a fixture process registered in the broker and assert recovery.
- Confirm controller ignores a same-name executable at an unexpected path.

Tests must avoid suspending the developer's real browser. Integration tests use dedicated fixture processes only.

### Manual system matrix

- Windows 10 and Windows 11.
- Chrome/Edge/Firefox representative sessions.
- Multiple windows with mixed minimized/restored state.
- Active audio and default audio-device changes.
- Logoff/shutdown and installer upgrade paths.

## Boundaries

### Always

- Resume tracked processes on every controlled shutdown path.
- Verify user, session, path, and process creation identity before process control.
- Choose no suspension when classification or recovery registration fails.
- Add a failing test before changing policy or recovery behavior.
- Build and run CTest after each implementation slice.
- Report benchmark conditions and absolute measurements.

### Ask first

- Add a third-party runtime or test dependency.
- Change supported Windows versions or architectures.
- Add a browser extension, Windows service, telemetry, networking, or auto-update.
- Change public command-line options incompatibly.
- Publish releases, push branches, or change GitHub repository settings.

### Never

- Commit secrets or signing keys.
- Suspend arbitrary non-browser processes.
- Treat forced process termination as a successful normal shutdown.
- Disable a failing test or compiler warning to make CI green.
- Claim that working-set reduction equals deallocated private memory.
- Run destructive integration tests against the user's real browser session.

## Success Criteria

1. A controller toggle causes a graceful guard exit and all fixture processes are resumed before exit.
2. Unexpected guard termination triggers broker recovery for registered fixture processes without resuming a PID-reused or identity-mismatched process.
3. Mixed window state tests prove that one restored visible window prevents minimized-only suspension of that browser family.
4. Default application and installer behavior is minimized-only; aggressive suspension requires explicit opt-in.
5. Unit and integration tests pass locally through CTest and on GitHub Actions Windows runners.
6. Release builds complete with the configured warning policy and no project warnings.
7. Benchmark output includes repeated trials, absolute values, variance/spread, resume latency, and page-fault cost; results are reproducible from documented commands.
8. README quick start contains only repository-relative links and clearly separates guarantees, experimental behavior, and limitations.
9. Architecture and safety ADRs explain why graceful IPC, identity validation, a recovery broker, and pure policy extraction were chosen.
10. An reviewer can follow `docs/project-demo.md` to understand and demonstrate the project in 10–15 minutes.

## Delivery Order

1. Add test harness and characterize current policy/default behavior.
2. Extract policy and fix mixed-window classification through RED/GREEN/REFACTOR.
3. Add graceful shutdown IPC and controller identity verification.
4. Make installer and upgrade behavior conservative and graceful.
5. Add broker protocol and crash-recovery integration test.
6. Add structured decision reasons and improve observability.
7. Redesign benchmark and replace sample claims.
8. Complete CI, open-source documentation, ADRs, and project demo.
9. Run full clean build/test/manual verification and prepare a release checklist.

## Risks and Mitigations

- **Undocumented suspend APIs:** keep experimental labeling, opt-in aggressive mode, fail closed, and recover independently.
- **PID reuse:** validate creation time and executable identity before resume/control.
- **IPC spoofing:** scope named objects to the interactive user/session and apply explicit security descriptors.
- **Tests freezing user software:** use purpose-built fixture processes only.
- **Windows runner limitations:** keep unit tests pure; mark truly interactive system tests for local/manual execution when CI cannot host them reliably.
- **Scope expansion into browser semantics:** document native blind spots and defer per-tab guarantees to the extension roadmap.

## Open Questions for Owner Approval

1. Should v1 include the crash-recovery broker, or should it be a documented v1.1 milestone after safe shutdown, tests, and CI?
2. Is Windows 10 x64 still a required target, or may the project target Windows 11 only?
3. Should the existing CLI option names remain fully backward compatible?
4. Is a 10–15 minute English project demo the desired deliverable, or should the project include both English and Traditional Chinese presentation material?
