# SPICEUnion

SPICEUnion is a C++17 rewrite of the interactive Spectre execution layer that
currently lives in `~/my_lab/projects/spectre_materials/src/spectre_interactive/`.

The project goal is to turn the existing Python execution path into a compact,
testable, embeddable simulator infrastructure library:

```text
states
  -> evaluator facade
  -> ordered concurrent worker pool
  -> simulator sessions
  -> worker work directories
  -> caller-owned parsing / result handling
  -> ordered results
```

## Current Status

The repository is in M0: design finalization and project bootstrap.

At this stage the priority is not to start with a real Spectre subprocess. The
first priority is to freeze the execution-layer contract, create the CMake /
GoogleTest skeleton, and validate scheduling behavior with fake or scripted
sessions.

## Scope

V1 focuses on the execution layer:

- evaluator options and batch execution facade;
- isolated evaluator namespaces and worker directories;
- ordered result collection, independent of task completion order;
- idle-worker queue scheduling instead of static round-robin dispatch;
- worker startup failure propagation and cleanup;
- per-task failure isolation;
- explicit cleanup and worker reload;
- Spectre interactive session support after the fake-session contract tests are
  stable;
- low-level result helpers and native PSF parsing after the execution core is
  stable.

V1 does not own:

- project-specific circuit metrics;
- optimization strategies;
- objective functions or scoring logic;
- experiment orchestration;
- PDK content management;
- GUI work;
- distributed scheduling.

Those responsibilities belong to upstream projects.

## Behavior Baseline

The behavior baseline is the current Python package:

- `~/my_lab/projects/spectre_materials/src/spectre_interactive/generic_evaluator.py`
- `~/my_lab/projects/spectre_materials/src/spectre_interactive/daemon_pool.py`
- `~/my_lab/projects/spectre_materials/src/spectre_interactive/spectre_daemon.py`
- `~/my_lab/projects/spectre_materials/src/spectre_interactive/task_library.py`

SPICEUnion should preserve the useful contract of
`GenericEvaluator.run(states, parse_func)` without copying Python's exact module
layout.

## Repository Layout

Current documents:

- `TODO`: active M0 tasks and deferred work.
- `doc/CPP版本开发计划书.md`: architecture, milestones, contract, and acceptance
  criteria.
- `doc/开发路线图.md`: stage-by-stage implementation tasks, file outputs,
  test outputs, completion definitions, and commit boundaries.
- `doc/简历亮点解析.md`: interview-facing story and resume positioning.
- `doc/README.md`: document directory ownership and naming rules.

Planned implementation layout:

```text
SPICEUnion/
├── CMakeLists.txt
├── include/su/
├── src/
│   ├── core/
│   ├── pool/
│   ├── session/
│   └── parse/
├── bindings/
│   └── python/
├── tests/
├── bench/
└── scripts/
```

## M0 Start Criteria

M0 is complete when:

- the root README and TODO describe the project state clearly;
- the development plan no longer uses migration wording from `spectre_materials`;
- the C++ core, C ABI, and Python binding boundaries are specified;
- a minimal CMake project can configure, build, and run a smoke test;
- fake or scripted session tests cover namespace isolation, startup failure,
  ordered result collection, idle-worker scheduling, and per-task failure
  isolation.

## Notes

This repository is currently not initialized as a Git repository. Once Git is
enabled, local commits should be made after code or project-structure changes.
