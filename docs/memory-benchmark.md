# Memory Benchmark

Generated: 2026-04-21 12:39:46

This report compares supported browser memory usage before and after browser_guard was running with aggressive memory mode enabled.

## Method

- Sample window per phase: 12 seconds
- Sample interval: 1000 ms
- Guard command: browser_guard.exe --aggressive-memory --trim-interval-ms 3000
- Browsers included: chrome, msedge, firefox, brave, opera, vivaldi
- Recommended test condition: keep the browser open, then focus another app during the guarded phase

## Summary

| Phase | Avg process count | Avg working set (MB) | Avg private bytes (MB) |
| --- | ---: | ---: | ---: |
| Baseline | 39 | 8.05 | 6991.09 |
| Guarded | 39 | 0.31 | 6991.11 |

## Delta

| Metric | Delta (MB) | Percent |
| --- | ---: | ---: |
| Working set | -7.74 | -96.15% |
| Private bytes | 0.02 | 0% |

## Interpretation

- Negative delta means memory usage dropped while browser_guard was running.
- Working set usually reacts faster than private bytes because trimming removes resident pages first.
- If the browser stayed focused or kept active audio output, the reduction can be small by design.
