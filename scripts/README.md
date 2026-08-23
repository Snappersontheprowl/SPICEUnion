# scripts

本目录存放开发脚本与 demo 脚本。

脚本应是小型、可复现的 wrapper，用于封装已文档化的构建、测试或环境检查命令。

## 当前脚本

- `verify_all_presets.sh`：一键本地验证全部 CMake 预设（default / external /
  libpsf / external-libpsf / python / python-libpsf-pic），作为开发期自检入口。
