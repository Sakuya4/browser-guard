# Changelog

All notable changes follow the spirit of [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.2.0] - 2026-07-13

### Added

- Independent crash-recovery broker with an identity-validated recovery journal.
- CTest unit and Windows integration tests.
- Strict Windows Debug/Release CI using MSVC `/W4 /WX`.
- Architecture, safety, ADR, contribution, security, and interview documentation.
- Counterbalanced benchmark with CPU, page-fault, and recovery-latency measurements.
- Explainable suspension decisions for foreground, audio, grace periods, and window state.

### Changed

- Minimized-only eligibility now requires every visible browser window to be minimized.
- Controller shutdown now uses authenticated per-session IPC and waits for normal cleanup.
- Controller process discovery validates executable path, Windows user, and session.
- Startup installation defaults are conservative; aggressive suspension is explicit opt-in.
- Installer upgrades and uninstalls fail safely if graceful shutdown cannot complete.

### Removed

- Forced controller termination of the guard.
- The unverified single-run `96.15%` working-set result as product evidence.
