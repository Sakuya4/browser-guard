# Interview Demo (10–15 minutes)

## 1. Problem and boundary (2 minutes)

Explain that background browsers can retain resident pages while memory-heavy foreground work needs RAM. Then state the key limitation: a native process manager does not understand tab semantics, so safety and recovery matter more than maximizing a percentage.

Show the product promise:

> Reduce background browser resource pressure without leaving the browser frozen or hiding the trade-offs.

## 2. Architecture (3 minutes)

Open `docs/architecture.md` and explain the three-process design:

- Guard: policy and Windows process control.
- Controller: authenticated graceful shutdown.
- Recovery broker: independent crash recovery.

Highlight the recovery transaction: identity record is atomically persisted before suspend, then removed only after successful resume.

## 3. C and Windows engineering (3 minutes)

Walk through:

- `src/process_control.c`: Tool Help, access tokens, Core Audio, PSAPI, EcoQoS, ntdll dynamic loading.
- `src/suspend_policy.c`: pure logic extracted from Win32 side effects.
- `src/control_protocol.c`: current-user DACL and session-local event.
- `src/recovery_journal.c`: atomic file replacement and PID-reuse identity.
- `src/recovery_broker.c`: independent process-liveness wait and recovery.

Discuss bounded storage, handle cleanup, safe defaults, and why `NtSuspendProcess` is explicitly labeled experimental.

## 4. Tests and failure injection (3 minutes)

Run:

```powershell
cmake -S . -B build -A x64 -DBG_WARNINGS_AS_ERRORS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Explain two important tests:

- mixed minimized/restored windows prevent family suspension;
- crash-recovery fixture starts suspended, its owner exits, and only the broker can resume it to write the success marker.

The fixtures avoid touching a real browser.

## 5. Evidence and trade-offs (2 minutes)

Show `tools/compare-memory.ps1`. Explain AB/BA counterbalancing, trial-level mean/SD, CPU, page faults, and recovery latency. Point out that working-set reduction is not private-memory deallocation and can create resume costs.

## 6. Close (1 minute)

Summarize what the project demonstrates:

- practical C11 module boundaries;
- Windows security and process identity;
- failure-safe multi-process lifecycle design;
- TDD and injected crash recovery;
- honest performance measurement;
- documented architectural trade-offs.

End with the roadmap: remove periodic resume heartbeat, persist background policy metadata, and optionally add a browser extension for tab-level semantics.
