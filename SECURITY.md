# Security Policy

## Supported version

Security fixes target the latest release and the `main` branch.

## Reporting a vulnerability

Please use the repository's **Security → Report a vulnerability** workflow to submit a private GitHub Security Advisory. Do not open a public issue for a vulnerability that could leave user processes suspended, control an unintended process, bypass identity checks, or weaken recovery data integrity.

Include the Windows version, browser, reproduction steps, expected behavior, observed behavior, and whether a process remained suspended after the guard exited.

## Security boundaries

`browser_guard` is a per-user desktop utility. It is not a sandbox, endpoint-security product, anti-malware tool, elevated service, or cross-user administrator. Its process-suspension mechanism is experimental and depends on undocumented Windows APIs. See [docs/safety.md](docs/safety.md).
