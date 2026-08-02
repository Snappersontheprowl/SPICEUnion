# src/pool

Ordered worker scheduling and simulator session pool implementation lives here.

The pool layer owns idle-worker dispatch, result ordering, and lifecycle cleanup
coordination. It should not know Spectre protocol details.
