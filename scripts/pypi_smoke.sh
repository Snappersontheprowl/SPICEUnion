#!/usr/bin/env bash
# 安装包模式冒烟：在干净 venv 中安装 PyPI 包（或本地源码），用仓库测试脚本验证。
#
# 用法：
#   scripts/pypi_smoke.sh                 # 安装 PyPI 最新版
#   scripts/pypi_smoke.sh 0.2.0           # 安装指定版本
#   scripts/pypi_smoke.sh --from-source   # 安装当前源码（pip install .）
#   scripts/pypi_smoke.sh 0.2.0 --with-external
#       # 追加真实仿真使用路径（ngspice；若设置 SPICEUNION_SPECTRE_MATERIALS_DIR
#       # 且本机有 spectre，则包含 AMP/dc 真实仿真）
#
# 关键点：不设置 PYTHONPATH，确保测试跑在“已安装的包”上。
set -euo pipefail

cd "$(dirname "$0")/.."

version=""
from_source=0
with_external=0
for arg in "$@"; do
  case "$arg" in
    --from-source) from_source=1 ;;
    --with-external) with_external=1 ;;
    -h|--help)
      sed -n '2,16p' "$0"
      exit 0
      ;;
    *) version="$arg" ;;
  esac
done

runtime_root="local/runtime/pypi_smoke"
venv="$runtime_root/venv"
mkdir -p "$runtime_root"

# 可用 PYPI_SMOKE_PYTHON 指定解释器（自托管机器上的 python3 可能过旧、
# 无法安装 cp39–cp312 的 wheel）；默认 python3。
python_bin="${PYPI_SMOKE_PYTHON:-python3}"

if [ ! -x "$venv/bin/python" ]; then
  "$python_bin" -m venv "$venv"
fi

"$venv/bin/python" -m pip install --upgrade pip >/dev/null

if [ "$from_source" -eq 1 ]; then
  "$venv/bin/pip" install . >/dev/null
elif [ -n "$version" ]; then
  "$venv/bin/pip" install "spiceunion==$version" >/dev/null
else
  "$venv/bin/pip" install spiceunion >/dev/null
fi

P="$venv/bin/python"
"$P" -c "import spiceunion; print('installed:', spiceunion.version())"

echo "==== 契约与读取 ===="
"$P" bindings/python/tests/test_import.py
"$P" bindings/python/tests/test_api_contract.py
"$P" bindings/python/tests/test_workflow_api_contract.py
"$P" bindings/python/tests/test_metrics_api_contract.py
"$P" bindings/python/tests/test_read_results.py tests/fixtures
"$P" bindings/python/examples/read_fixture_results.py tests/fixtures
"$P" bindings/python/examples/metrics_only.py

echo "==== 使用路径 ===="
if [ "$with_external" -eq 1 ]; then
  SPICEUNION_ENABLE_PYTHON_WORKFLOW_EXTERNAL_TESTS=1 "$P" bindings/python/tests/test_user_journey.py
  SPICEUNION_ENABLE_PYTHON_WORKFLOW_EXTERNAL_TESTS=1 "$P" bindings/python/tests/test_workflow_external.py
else
  "$P" bindings/python/tests/test_user_journey.py
fi

echo "==== doctor ===="
"$venv/bin/spiceunion" doctor

echo "pypi smoke 完成。"
