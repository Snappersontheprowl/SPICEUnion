# tests

GoogleTest-based tests live here.

## Test Groups

- Smoke tests verify the build and link path.
- Contract tests verify execution-layer behavior with fake or scripted sessions.
- External tests may require Spectre or PDK access and must be disabled by
  default unless explicitly enabled.

## Commands

Default tests:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

External Spectre lifecycle tests:

```bash
cmake --preset external
cmake --build --preset external
ctest --preset external
```

External tests currently cover:

- Spectre interactive handshake and stop;
- single-session `(sclRun "all")`;
- default Spectre evaluator multi-worker batch.
