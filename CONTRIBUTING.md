# Contributing

Thank you for improving `browser_guard`. Because the project controls live processes, safety regressions are treated as release blockers.

## Development setup

```powershell
cmake -S . -B build -A x64 -DBG_WARNINGS_AS_ERRORS=ON
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

Before opening a pull request, run both Debug and Release builds when changing Win32 lifecycle, recovery, or installer behavior.

## Change requirements

- Write or update a test before changing policy or recovery behavior.
- Use dedicated fixture processes; never suspend a contributor's real browser in an automated test.
- Default to no suspension when inspection, identity validation, journaling, or recovery setup fails.
- Keep normal shutdown free of `TerminateProcess` and `Stop-Process -Force`.
- Preserve existing CLI behavior unless the change includes a migration plan.
- Document significant decisions in `docs/decisions/`.
- Report benchmark conditions and absolute measurements; do not equate working-set reduction with private-memory deallocation.

## Pull requests

Keep pull requests focused and describe:

1. the failure mode or user outcome being changed;
2. the safety boundary affected;
3. tests that failed before and pass after the change;
4. manual Windows/browser checks performed;
5. rollback implications.

CI must pass MSVC `/W4 /WX` builds and all CTest targets.
