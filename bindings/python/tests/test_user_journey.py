"""用户视角的使用路径测试（公开 API only）。

两种运行方式共用本文件：

1. 源码构建模式：`ctest --preset python`（PYTHONPATH 指向 build 目录）；
2. 安装包模式：`pip install spiceunion` 后直接运行（无 PYTHONPATH，见
   `scripts/pypi_smoke.sh`）。

默认只做“无 EDA 依赖”的部分（doctor 探测）。真实仿真需要显式开启：

    SPICEUNION_ENABLE_PYTHON_WORKFLOW_EXTERNAL_TESTS=1

并满足对应工具/材料条件；不满足时按 skip 处理并打印原因。

真实 Spectre 材料通过中性变量提供：

    SPICEUNION_SPECTRE_MATERIALS_DIR=<含 external/netlist 与 external/pdk 的目录>
"""

import os
import shutil
import sys
from pathlib import Path

import spiceunion as su


def external_enabled() -> bool:
    return os.environ.get("SPICEUNION_ENABLE_PYTHON_WORKFLOW_EXTERNAL_TESTS") == "1"


def ngspice_available() -> bool:
    explicit = os.environ.get("SPICEUNION_NGSPICE")
    if explicit and os.access(explicit, os.X_OK):
        return True
    return shutil.which("ngspice_con") is not None or shutil.which("ngspice") is not None


def spectre_available() -> bool:
    explicit = os.environ.get("SPICEUNION_SPECTRE")
    if explicit and os.access(explicit, os.X_OK):
        return True
    return shutil.which("spectre") is not None


def spectre_amp_dc_netlist() -> Path | None:
    root = os.environ.get("SPICEUNION_SPECTRE_MATERIALS_DIR")
    if not root:
        return None
    netlist = Path(root) / "external" / "netlist" / "AMP" / "dc" / "input.scs"
    return netlist if netlist.is_file() else None


def run_ngspice_journey() -> None:
    with su.Simulation(
        netlist_path="ngspice_builtin.cir",
        simulator="ngspice",
        ngspice_task="rc_tran",
        result_format="nspice_wrdata",
        workers=1,
        work_dir_base="local/runtime/python_user_journey",
        metrics=[{"kind": "tran", "signal": "v(out)", "derived": ["settling_time"]}],
    ) as simulation:
        simulation.add_parameter("resistance_ohm", default_value=1000.0)
        simulation.add_parameter("capacitance_f", default_value=1e-12)
        results = simulation.run([{}])

    assert len(results) == 1
    result = results[0]
    assert result.ok(), result.message
    assert result.metrics_ok(), result.metrics_message()
    settling = result.metric("settling_time_s")
    assert settling is not None and settling > 0
    assert result.read_tran("v(out)").ok()
    print(f"ngspice journey ok: settling_time_s={settling:.3e}")


def run_spectre_journey(netlist: Path) -> None:
    with su.Simulation(
        netlist_path=str(netlist),
        simulator="spectre",
        workers=1,
        timeout_seconds=120,
        work_dir_base="local/runtime/python_user_journey_spectre",
    ) as simulation:
        results = simulation.run([{}])

    assert len(results) == 1
    result = results[0]
    assert result.ok(), result.message
    print("spectre journey ok: AMP/dc 真实仿真完成")


def main() -> None:
    report = su.doctor()
    assert "Spectre" in report and "Ngspice" in report

    if not external_enabled():
        print(
            "skip: set SPICEUNION_ENABLE_PYTHON_WORKFLOW_EXTERNAL_TESTS=1 "
            "to run real-simulator journey"
        )
        return

    if ngspice_available():
        run_ngspice_journey()
    else:
        print("skip: ngspice executable is not available in PATH")

    netlist = spectre_amp_dc_netlist()
    if spectre_available() and netlist is not None:
        run_spectre_journey(netlist)
    else:
        print(
            "skip: spectre or SPICEUNION_SPECTRE_MATERIALS_DIR "
            "(external/netlist + external/pdk) is not available"
        )


if __name__ == "__main__":
    main()
    sys.exit(0)
