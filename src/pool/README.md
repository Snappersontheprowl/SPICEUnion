# src/pool

本目录存放有序 worker 调度与 simulator session pool 实现。

pool 层负责 idle-worker dispatch、result ordering 与 lifecycle cleanup 协调。
它不应知道 Spectre protocol 细节。
