# ADR-003: Extract a conservative, pure suspension policy

## Status

Accepted

## Date

2026-07-12

## Context

A single `is_minimized` flag represented an entire executable-name browser family. With multiple windows, scanning one minimized window could mark the family as minimized even when another restored window remained visible.

## Decision

Window scanning records minimized and visible-restored counts separately. A pure C policy function defines minimized-only eligibility: at least one visible window, all visible windows minimized, and zero visible restored windows. Unknown or empty inspection fails closed.

The installer explicitly selects minimized-only policy. Aggressive background suspension remains opt-in.

## Alternatives considered

### Last observed window wins

Rejected because enumeration order is not a safe policy input.

### Per-PID window policy

Insufficient because Chromium process roles do not map cleanly to user-visible windows or tabs.

### Per-tab browser extension

The safer long-term direction, but deferred to keep v1 focused on native C and recovery correctness.

## Consequences

- Mixed-window behavior is deterministic and unit-testable.
- One visible restored window protects the entire browser family, reducing memory savings in favor of safety.
- Executable-family grouping remains a documented limitation.
