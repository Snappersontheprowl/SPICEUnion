#!/usr/bin/env bash
# 一键本地验证全部 CMake 预设（开发期自检，不依赖 GitHub）。
set -euo pipefail

cd "$(dirname "$0")/.."

presets=(default external libpsf external-libpsf python python-libpsf-pic)

for preset in "${presets[@]}"; do
  echo "==== configure: ${preset}"
  cmake --preset "${preset}"
  echo "==== build: ${preset}"
  cmake --build --preset "${preset}"
  echo "==== test: ${preset}"
  ctest --preset "${preset}" --output-on-failure
done

echo "全部预设验证通过。"
