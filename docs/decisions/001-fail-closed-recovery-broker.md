# ADR-001: Require independent recovery before process suspension

## Status

Accepted

## Date

2026-07-12

## Context

The guard uses process-wide suspension. In-process cleanup cannot run after a crash or forced termination, so a browser could remain suspended after the guard disappeared. PID reuse also makes a plain PID list unsafe.

## Decision

Every suspend must first persist a recovery record containing PID, process creation time, session ID, and executable path. An independent broker must signal readiness before suspension is enabled. After guard exit, the broker revalidates the live identity before resuming. If journaling or broker startup fails, suspension is refused.

## Alternatives considered

### In-process cleanup only

Rejected because crashes bypass cleanup.

### Controller performs best-effort resume

Rejected because the guard can crash without the controller running, and the controller would not have durable identity state.

### Windows service

Deferred because it expands privilege, installation, and cross-user security scope beyond a per-user portfolio release.

### Shared memory only

Rejected for the first recovery implementation because durable atomic file replacement also covers abrupt process loss and is easier to inspect during incident diagnosis.

## Consequences

- Suspension becomes fail-closed.
- The release contains a third executable.
- Installation and uninstallation must manage broker lifecycle.
- Journal format and identity validation become security-sensitive interfaces.
- Recovery guarantees unfreezing; full restoration of every background policy remains future work.
