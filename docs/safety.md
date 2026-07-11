# Safety Model

## Safety objective

The primary safety objective is stronger than memory reduction:

> A failure in browser_guard must not knowingly leave a validated browser process suspended without an independent recovery path.

## Guarantees implemented by v1

- Only explicitly supported browser executable names are considered by the guard.
- The guard filters to the current Windows user and logon session.
- Minimized-only mode refuses suspension when any visible restored window exists in the browser family.
- Foreground and active-output-audio families are protected.
- Suspension requires a successfully started independent broker.
- Suspension requires an atomically persisted recovery record.
- Recovery validates creation time and full process identity before resume.
- Controller, installer upgrade, and uninstall use graceful shutdown.
- A shutdown timeout is surfaced as failure; it is not converted into forced termination.

## What is not guaranteed

The native agent cannot reliably identify:

- downloads or uploads;
- muted video or silent live streams;
- WebRTC meetings without active output audio;
- microphone or screen capture;
- unsaved forms and `beforeunload` state;
- Jupyter/Colab computation;
- service workers and notifications;
- WebUSB, WebSerial, SSH, remote administration, or other long-lived tab work.

Therefore aggressive suspension can interrupt valuable work even when the program behaves exactly as implemented. Users should keep the default minimized-only policy and opt into aggressive suspension only with this limitation understood.

## Undocumented Windows API risk

Suspension uses dynamically resolved `NtSuspendProcess` and `NtResumeProcess`. These calls can change across Windows versions, be denied by protected processes, or trigger endpoint-security scrutiny. They are treated as an experimental mechanism, not a stable consumer-product contract.

## Failure analysis

| Failure | Response |
| --- | --- |
| Controller cannot signal guard | Report failure; do not terminate. |
| Guard cannot start broker | Continue without process suspension. |
| Journal write fails | Refuse that suspend operation. |
| Suspend fails after registration | Remove the record and report in verbose mode. |
| Normal resume fails | Keep the record for broker recovery. |
| Guard crashes | Broker validates and resumes journal records. |
| PID has been reused | Creation-time/path/user/session mismatch; do not resume. |
| Journal is malformed | Recovery fails closed and retains evidence for diagnosis. |

## Operational guidance

- Save important browser work before evaluating aggressive mode.
- Use minimized, idle browser windows for benchmarks.
- Keep all three executables together.
- Do not delete the recovery executable or journal while the guard is running.
- If a browser remains unresponsive, run `browser_guard_control.exe --shutdown`, then inspect Task Manager and the recovery journal before restarting the guard.

## Future safety work

Tab-level safety requires browser cooperation. An optional WebExtension/native-messaging design could protect pinned tabs, downloads, forms, meetings, and recent activity before any OS-level action. That architecture is intentionally deferred until the native recovery core is stable.
