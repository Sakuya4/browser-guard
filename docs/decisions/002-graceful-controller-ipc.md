# ADR-002: Use authenticated named-event shutdown

## Status

Accepted

## Date

2026-07-12

## Context

The original controller used `TerminateProcess`, bypassing the guard's resume cleanup. It also discovered targets only by `browser_guard.exe` filename.

## Decision

The guard creates a session-local named event with an explicit DACL for the current Windows user. The controller verifies the expected sibling path, current user, and logon session, signals the event, and waits for normal exit. Timeout is reported as failure and never converted to normal forced termination.

## Alternatives considered

### `WM_CLOSE`

Rejected because the guard's overlay is not a stable public control endpoint and window messages provide weaker identity and lifecycle semantics.

### Named pipe

Viable for richer commands, but unnecessary for a one-way shutdown request in v1.

### `GenerateConsoleCtrlEvent`

Rejected because the production executable is a GUI subsystem application and console attachment is fragile.

## Consequences

- Shutdown has a clear, testable protocol.
- Duplicate guard instances are rejected when the event already exists.
- Future controller commands may justify migration to a named pipe, but shutdown remains intentionally small.
