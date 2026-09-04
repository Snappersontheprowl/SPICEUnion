"""SPICEUnion 命令行入口。

安装 wheel 后可用：

    spiceunion doctor

或直接运行模块（等价）：

    python -m spiceunion_cli doctor

默认（不带子命令）输出版本号与仿真器探测报告。
"""

import sys

import spiceunion


def main(argv=None) -> int:
    args = list(argv if argv is not None else sys.argv[1:])
    if args and args[0] == "doctor":
        sys.stdout.write(spiceunion.doctor())
        return 0
    if args:
        sys.stderr.write("usage: spiceunion [doctor]\n")
        return 2
    sys.stdout.write(f"spiceunion {spiceunion.version()}\n\n")
    sys.stdout.write(spiceunion.doctor())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
