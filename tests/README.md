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
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

External Spectre lifecycle tests:

```bash
cmake -S . -B cmake-build-external -DSPICEUNION_ENABLE_EXTERNAL_TESTS=ON
cmake --build cmake-build-external
ctest --test-dir cmake-build-external --output-on-failure
```
