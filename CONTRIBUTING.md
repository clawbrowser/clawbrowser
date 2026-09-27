# Contributing to Clawbrowser

Thank you for helping improve Clawbrowser. Contributions of all sizes are welcome: bug reports, documentation fixes, tests, design improvements, and new features.

Please keep contributions focused, factual, and easy to review. By participating, you agree to follow our [Code of Conduct](CODE_OF_CONDUCT.md).

## Before you start

- Search existing issues and pull requests to avoid duplicate work.
- For a small bug fix, documentation correction, or test improvement, feel free to open a pull request directly.
- For a large feature, architectural change, or new dependency, open an issue first so the approach can be discussed before significant work begins.
- Do not report security vulnerabilities publicly. Follow [SECURITY.md](.github/SECURITY.md).

## Development setup

Fork the repository and clone your fork:

```
git clone https://github.com/YOUR-USERNAME/clawbrowser.git
cd clawbrowser
git remote add upstream https://github.com/clawbrowser/clawbrowser.git
git fetch upstream
```

Build and run instructions are documented in [INSTALL.md](INSTALL.md), and a container-based setup is available via the provided `Dockerfile` and `docker/` directory. Create a focused branch from the latest default branch before making changes:

```
git switch -c fix/short-description
```

## Repository structure

- `clawbrowser/` — core application source.
- `api/` — service and API layer.
- `scripts/` — build and maintenance helpers.
- `docker/` — container build and runtime configuration.
- `docs/` — project documentation.
- `assets/` and `branding/` — application assets and branding.

## Making changes

- Keep each contribution limited to one coherent concern.
- Match the existing code style and conventions of the files you touch.
- Preserve existing behavior unless the change intentionally replaces it.
- Add or update tests for bug fixes and behavior changes where applicable.
- Avoid unrelated formatting or dependency updates.
- Never commit credentials, API keys, personal data, dependency directories, or generated build output.

## Commits

Write concise commit messages in the imperative mood. A useful message explains the outcome rather than the activity:

```
Fix proxy verification order
Reconcile managed profiles with installed fonts
Add packaging coverage for Linux
```

Keep unrelated changes out of the final history. Do not rewrite history after review has started unless the reviewers expect it.

## Pull requests

A pull request should include:

- a short explanation of the problem and the chosen solution;
- links to related issues;
- the checks you ran and their results;
- screenshots or a short recording for visible UI changes;
- notable risks, limitations, or follow-up work.

Before requesting review, confirm that:

- the change is focused and contains no accidental files;
- relevant tests and builds pass;
- no secrets, personal data, or generated build output are included.

Reviewers may request changes for correctness, maintainability, consistency, security, or scope. Please keep discussion constructive.

## Reporting bugs

A useful bug report includes the Clawbrowser version, operating system, reproduction steps, expected behavior, actual behavior, and relevant logs or screenshots with sensitive information removed.

Thank you for making Clawbrowser better.
