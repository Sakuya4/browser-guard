# Architecture

## Design goal

`browser_guard` separates policy, Windows side effects, control, and recovery so that a failure in the main guard does not strand a browser in the suspended state.

```text
Desktop Toggle / installer
          |
          | authenticated shutdown event
          v
  browser_guard_control.exe
          |
          v
    browser_guard.exe  ---- atomic recovery journal ----+
          |                                             |
          | suspend/resume                              v
          +---- supported browser processes   browser_guard_recovery.exe
                                                        |
                                                        +-- validates identity and resumes after crash
```

## Components

### Guard

`src/browser_guard.c` owns the orchestration loop and tracked runtime state. It collects process groups, annotates their window/audio state, asks pure policy helpers for eligibility, persists recovery identity, then applies Windows process state.

The guard cannot suspend when its recovery broker failed to start or journaling failed. This is intentional fail-closed behavior.

### Process-control adapter

`src/process_control.c` isolates Tool Help enumeration, ownership checks, Core Audio enumeration, foreground-window discovery, memory priority, EcoQoS, working-set trimming, and the experimental ntdll suspend/resume calls.

### Pure suspension policy

`src/suspend_policy.c` contains side-effect-free policy logic. Window scanning records minimized and visible-restored window counts separately. Under minimized-only policy, a browser family is eligible only when it has at least one visible window, every visible window is minimized, and no restored visible window exists.

### Controller and control protocol

`browser_guard_control.exe` discovers only the guard at the expected sibling path and validates the current user/session before acting. Shutdown is requested through a `Local` named event whose DACL grants access only to the current Windows user. The controller waits for normal guard exit and reports timeout instead of force-terminating.

### Recovery journal

Before suspension, the guard atomically persists:

- PID;
- process creation time;
- Windows session ID;
- full executable path.

Writes go to a temporary file, are flushed, and replace the journal atomically. An empty journal is deleted. A record is removed only after normal resume succeeds.

### Recovery broker

The broker opens a synchronization handle to the guard and signals readiness before suspension is enabled. When the guard exits, the broker loads the journal and revalidates path, user, session, and creation time. A mismatched or reused PID is never resumed. Valid records are resumed through `NtResumeProcess` and the resolved journal is removed.

## Lifecycle

### Startup

1. Build current-user/session security context.
2. Create the authenticated shutdown event; failure also prevents duplicate guard instances.
3. Resolve any stale recovery journal.
4. Initialize an empty journal.
5. Start the recovery broker and wait for readiness.
6. Initialize COM and overlay state.
7. Enter the policy loop.

If step 3–5 fails, the guard may still apply non-suspending background policies, but it refuses process suspension.

### Suspend

1. Discover and classify a supported browser family.
2. Evaluate foreground, audio, multi-window, grace, and user policy.
3. Persist and flush the recovery identity.
4. Suspend the exact process.
5. If suspension fails, remove the journal entry.

### Normal shutdown

1. Controller signals the named event.
2. Guard leaves its loop.
3. Guard resumes every tracked suspended process.
4. Successful resumes remove their journal records.
5. Guard exits; broker observes an empty journal and exits.

### Unexpected shutdown

1. Broker observes the guard process handle becoming signaled.
2. Broker loads the last atomically committed journal.
3. Each record is identity-validated to prevent PID reuse.
4. Valid processes are resumed independently of the guard.

## Trust boundaries

- Browser/window/audio observations are untrusted and incomplete; uncertainty means no suspension.
- Named control objects are session-local and current-user ACL protected.
- Recovery journal content is never trusted without live process identity validation.
- CLI input is bounded and validated by `app_config.c`.
- Automated tests never target real browser processes.

## Known architectural debt

- `NtSuspendProcess` and `NtResumeProcess` are undocumented.
- Browser-family grouping is still executable-name based, not profile or tab based.
- The legacy heartbeat temporarily resumes a suspended process to probe a window.
- Recovery currently guarantees unfreezing but does not persist every memory-priority/EcoQoS setting needed for perfect policy restoration.
- `MAX_PATH` bounds apply to recorded executable paths.
