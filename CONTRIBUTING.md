# Contributing to OpenSMA

> Important: We are not currently accepting external code contributions. Please do not open pull requests. Bug reports and feature requests via Issues are welcome. PRs from external contributors may be closed without review.

Thanks for your interest in contributing! This project is licensed under Apache License 2.0. By participating, you agree your contributions are licensed under the same terms.

## Ways to contribute
- Report bugs and request features via Issues
- Improve documentation (via Issue suggestions; external PRs are not accepted)
- Submit code changes via Pull Requests (PRs) — maintainers/internal contributors only

## Workflow
Note: The following workflow applies to maintainers/internal contributors only. External PRs will be closed.

1. Fork the repository and create a topic branch from `main`.
2. Make your changes in small, focused commits.
3. Ensure builds succeed locally (see README for build instructions).
4. Format and lint your changes before submitting a PR.
5. Open a PR with a clear description, rationale, and testing notes.

### Commit sign-off (DCO)
This project uses the Developer Certificate of Origin (DCO). Each commit must be signed off using `-s`, which adds a `Signed-off-by` line matching your legal name and email.

Example:
```bash
git commit -s -m "feat: add new configuration validation"
```

DCO text: https://developercertificate.org/

### SPDX headers in source files
Include SPDX headers at the top of new or modified source files.

- C/C++ example:
```c
/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES
 * SPDX-License-Identifier: Apache-2.0
 */
```

- Python example:
```python
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES
# SPDX-License-Identifier: Apache-2.0
```

- Shell script example:
```bash
#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES
# SPDX-License-Identifier: Apache-2.0
```

### Code style and checks
- C/C++: use `clang-format` with the provided `.clang-format` file.
- Python: follow PEP 8 conventions.
- Keep code clear and well-documented where non-obvious.

Before submitting a PR:
- Build firmware as described in the README and ensure success.
- Run formatting where applicable (e.g., clang-format).
- Ensure new files have SPDX headers.

### Licensing
Unless explicitly stated otherwise, contributions are licensed under the Apache License, Version 2.0. See `LICENSE.md`.

### Security
Do not file public issues for potential security vulnerabilities. Please use GitHub's "Report a vulnerability" (Security Advisories) to contact the maintainers privately.
