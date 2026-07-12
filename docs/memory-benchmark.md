# Memory Benchmark

No verified product benchmark is published yet.

The earlier single-run `96.15%` working-set result was removed because its baseline was already unusually low, it did not use repeated or counterbalanced trials, and private bytes did not decrease. It is not used as product evidence.

Run the current AB/BA benchmark locally after building Release binaries:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\compare-memory.ps1
```

The generated report includes repeated-trial mean and sample standard deviation for working set, private bytes, CPU time, and page faults, plus graceful shutdown/resume latency. Results remain workload- and machine-specific.
