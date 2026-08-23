# Security Policy

## Supported Versions

We actively maintain and provide security patches for the following versions of `fastwad`:

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

## Reporting a Vulnerability

The security of `fastwad` is taken seriously. If you discover a potential security vulnerability (such as a buffer overflow, out-of-bounds read/write, integer overflow, memory corruption, or path traversal vulnerability), please **do not open a public issue**.

Instead, please report the vulnerability privately using one of the following methods:

1. **GitHub Private Vulnerability Reporting**: Submit an advisory via the [Security tab](https://github.com/urgorri/fastwad/security/advisories/new) on GitHub.
2. **Direct Contact**: Email the maintainer at `gaston@urgorri.com` (or the repository owner's listed profile email).

### What to include in your report

To help us triage and resolve the issue quickly, please provide:
- A clear description of the vulnerability and its potential impact.
- Exact steps to reproduce the issue (including sample malformed files or commands if applicable).
- Target OS, compiler, and version of `fastwad` tested.
- Any suggested fix or patch if available.

### Response Timeline

- **Initial Response**: Within 48 hours of receipt.
- **Triage & Assessment**: Within 5 business days.
- **Resolution & Patch**: Once validated, a fix will be published in a patch release, and the reporter will be credited in the security advisory (unless anonymity is requested).
