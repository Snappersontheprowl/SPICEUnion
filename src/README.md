# src

Implementation files live here.

## Layout

- `core/`: evaluator-facing core types and orchestration.
- `pool/`: ordered worker scheduling and session pool implementation.
- `session/`: simulator session implementations and protocol helpers.
- `parse/`: result readers and helper calculations.

## Naming Rules

- Directory names describe responsibility.
- Implementation files mirror the public header responsibility when practical.
