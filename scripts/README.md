# scripts

本目录存放开发脚本与 demo 脚本。

脚本应是小型、可复现的 wrapper，用于封装已文档化的构建、测试或环境检查命令。

## 当前脚本

- `verify_all_presets.sh`：一键本地验证全部 CMake 预设（default / external /
  libpsf / external-libpsf / python / python-libpsf-pic），作为开发期自检入口。
- `pypi_smoke.sh`：安装包模式冒烟——在干净 venv 中安装 PyPI 包（或 `--from-source`），
  用 `bindings/python/tests` 与 examples 验证；`--with-external` 追加真实仿真使用
  路径（ngspice；若提供 `SPICEUNION_SPECTRE_MATERIALS_DIR` 且有 spectre，则含
  AMP/dc 真实仿真）。
