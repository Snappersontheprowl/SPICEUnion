#!/usr/bin/env python3
"""Spectre interactive SKILL 探针（手动工具，不进入 ctest）。

用途：实验 B —— 判定常驻 interactive 会话中能否为每次 run 指定输出目录/结果名，
以及可用的 SKILL 函数名。

用法：
    # 1) 先做函数发现（默认脚本：apropos scl/output/result）
    python3 tests/manual/spectre_skill_probe.py

    # 2) 用候选命令做“两次 run 前后目录对比”
    python3 tests/manual/spectre_skill_probe.py --candidate '(sclSetOutputDir "/tmp/x")'

    # 3) 执行自定义 SKILL 命令脚本（每行一条）
    python3 tests/manual/spectre_skill_probe.py --script my_skill.txt

需要真实 Spectre 许可；结论回填 doc/develop_doc/20_专题记录/06_*.md 的「验证结果」。
"""

import argparse
import os
import queue
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

HANDSHAKE_MARKER = "Entering Skill interactive front end"
DEFAULT_DISCOVERY = [
    '(apropos "scl")',
    '(apropos "output")',
    '(apropos "result")',
]
DEFAULT_NETLIST = """simulator lang=spectre

parameters r=1k c=1p

Vin (in 0) vsource dc=0 mag=1
R1 (in out) resistor r=r
C1 (out 0) capacitor c=c

ac ac start=1Meg stop=10G dec=20
save out
"""


class SpectrePipe:
    def __init__(self, executable: str, netlist: Path, work_dir: Path):
        work_dir.mkdir(parents=True, exist_ok=True)
        self.process = subprocess.Popen(
            [executable, str(netlist), "+interactive", "-64", "-o", str(work_dir)],
            cwd=str(work_dir),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            bufsize=0,
        )
        self.lines: "queue.Queue[str]" = queue.Queue()
        self.transcript: list[str] = []
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    def _read_loop(self) -> None:
        assert self.process.stdout is not None
        pending = b""
        while True:
            chunk = self.process.stdout.read(1)
            if not chunk:
                break
            pending += chunk
            if chunk == b"\n":
                line = pending.decode("utf-8", errors="replace")
                pending = b""
                self.lines.put(line)
                self.transcript.append(line)

    def send(self, command: str) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write((command.rstrip("\n") + "\n").encode())
        self.process.stdin.flush()

    def wait_for(self, marker: str, timeout: float) -> bool:
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                line = self.lines.get(timeout=0.2)
            except queue.Empty:
                continue
            if marker in line:
                return True
        return False

    def drain(self, idle_seconds: float = 1.5) -> list[str]:
        collected: list[str] = []
        deadline = time.time() + idle_seconds
        while True:
            remaining = deadline - time.time()
            if remaining <= 0:
                break
            try:
                collected.append(self.lines.get(timeout=remaining))
                deadline = time.time() + idle_seconds
            except queue.Empty:
                break
        return collected

    def close(self) -> None:
        try:
            self.send("(sclQuit)")
            time.sleep(0.3)
        except Exception:
            pass
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()


def snapshot(root: Path) -> dict:
    entries = {}
    for path in sorted(root.rglob("*")):
        if path.is_file():
            stat = path.stat()
            entries[str(path.relative_to(root))] = (stat.st_size, stat.st_mtime_ns)
    return entries


def print_delta(before: dict, after: dict) -> None:
    added = [p for p in after if p not in before]
    changed = [p for p, meta in after.items() if p in before and before[p] != meta]
    print(f"  [差异] 新增 {len(added)} 个，变化 {len(changed)} 个")
    for path in added:
        print(f"    + {path}")
    for path in changed:
        print(f"    ~ {path}")
    if added and not changed:
        print("  [结论] 候选命令可能生效：第二次 run 写入了新路径")
    elif changed and not added:
        print("  [结论] 候选命令无效：仍是同名路径被重写")
    else:
        print("  [结论] 需人工判断（既无新增也无变化）")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--netlist", default="", help="自有网表（需含 ac 与 save）")
    parser.add_argument("--candidate", default="", help="两次 run 之间发送的 SKILL 命令")
    parser.add_argument("--script", default="", help="SKILL 命令脚本文件（每行一条）")
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()

    executable = os.environ.get("SPICEUNION_SPECTRE") or shutil.which("spectre")
    if not executable:
        print("未找到 spectre：设置 SPICEUNION_SPECTRE 或加入 PATH", file=sys.stderr)
        return 2

    run_root = (Path("local/runtime/spectre_skill_probe") / f"run_{int(time.time())}").resolve()
    work_dir = run_root / "worker_0"
    run_root.mkdir(parents=True, exist_ok=True)

    if args.netlist:
        netlist = Path(args.netlist).resolve()
    else:
        netlist = (run_root / "probe_input.scs").resolve()
        netlist.write_text(DEFAULT_NETLIST, encoding="utf-8")

    print(f"spectre: {executable}")
    print(f"网表: {netlist}")
    print(f"会话工作目录: {work_dir}")

    pipe = SpectrePipe(executable, netlist, work_dir)
    try:
        if not pipe.wait_for(HANDSHAKE_MARKER, args.timeout):
            print("握手失败，输出如下：")
            print("".join(pipe.transcript[-20:]))
            return 3
        pipe.send('(setq top (sclGetCircuit ""))')
        pipe.drain(1.0)

        if args.script:
            commands = [
                line.strip()
                for line in Path(args.script).read_text(encoding="utf-8").splitlines()
                if line.strip() and not line.strip().startswith("#")
            ]
        else:
            commands = DEFAULT_DISCOVERY

        print("\n[命令探查]")
        for command in commands:
            print(f"\n>>> {command}")
            pipe.send(command)
            for line in pipe.drain(1.5):
                print("   ", line.rstrip())

        if args.candidate:
            print("\n[实验 B] 两次 run 前后目录对比")
            candidate_dir = run_root / "job_000002"
            candidate_dir.mkdir(parents=True, exist_ok=True)
            print(">>> (sclRun \"all\")  # 第一次")
            pipe.send('(sclRun "all")')
            pipe.drain(3.0)
            before = snapshot(run_root)

            print(f"\n>>> 候选命令: {args.candidate}")
            pipe.send(args.candidate)
            for line in pipe.drain(1.5):
                print("   ", line.rstrip())

            print(">>> (sclRun \"all\")  # 第二次")
            pipe.send('(sclRun "all")')
            pipe.drain(3.0)
            after = snapshot(run_root)
            print_delta(before, after)
            print(f"  候选结果目录: {candidate_dir}")

        transcript_path = run_root / "transcript.log"
        transcript_path.write_text("".join(pipe.transcript), encoding="utf-8")
        print(f"\n完整交互日志: {transcript_path}")
        print("结论请回填专题 06「方案三 -> 验证结果」；机器细节写本地私有笔记。")
    finally:
        pipe.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
