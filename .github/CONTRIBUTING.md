# Contributing to SocketSpy

## Before You Start

- Read `.socketspy/MASTER_PROMPT.md` for architectural constraints.
- Check the open issues and project board before starting new work.
- Open an issue for significant changes before submitting a PR.

## Fork and Branch

1. Fork the repository on GitHub.
2. Create a branch from `main` (never commit directly to `main`).

Branch naming convention:

| Type | Pattern | Example |
|------|---------|---------|
| Agent feature | `agent/{name}` | `agent/core-ring-buffer` |
| Bug fix | `fix/{issue-number}` | `fix/42` |
| Protocol | `protocol/{name}` | `protocol/j1939-pgn` |
| Documentation | `docs/{topic}` | `docs/lua-api` |
| Refactor | `refactor/{topic}` | `refactor/dbc-ast` |

## Code Style

All C++ code must pass:

```bash
clang-format --dry-run --Werror $(find . -name '*.cpp' -o -name '*.h' | grep -v build/)
clang-tidy $(find . -name '*.cpp' | grep -v build/) -- -std=c++23
```

Run the formatter before committing:

```bash
find . -name '*.cpp' -o -name '*.h' | grep -v build/ | xargs clang-format -i
```

File size limits are enforced in CI:
- `.cpp` / `.c`: 300 lines maximum
- `.h` / `.hpp`: 150 lines maximum
- `CMakeLists.txt` (per directory): 100 lines maximum
- Shell scripts: 80 lines maximum

## PR Requirements

A pull request is mergeable when:

1. All CI checks pass (ubuntu-22.04 and ubuntu-24.04).
2. Zero new clang-format warnings.
3. Zero new clang-tidy warnings.
4. All new public functions have at least one unit test.
5. No outbound network calls are introduced (verified by strace check in CI).
6. Valgrind reports zero errors on the core binary.
7. The PR description explains *why* the change is needed, not just what it does.

## Running the Test Suite Locally

```bash
bash scripts/dev/setup_vcan.sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
bash scripts/dev/run_integration.sh
```

## Commit Messages

Use the imperative mood in the subject line. Keep the subject under 72 characters.
Add a blank line between the subject and body. Reference issues with `Fixes #N`.

Example:
```
implement SPSC ring buffer for CAN frame capture

Uses a fixed-size power-of-two buffer with cache-line-aligned head/tail.
Lock-free for single producer, single consumer.

Fixes #15
```

## Reporting Bugs

Use the **Bug Report** issue template. Include your distro, Qt version,
and CAN interface type.

## Security Issues

Do not open public issues for security vulnerabilities.
See [SECURITY.md](../SECURITY.md) for the reporting process.
