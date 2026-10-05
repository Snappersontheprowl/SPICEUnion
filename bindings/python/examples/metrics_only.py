"""只取指标的最小示例（Ngspice 内置 RC TRAN 任务）。

需要本机可用的 ngspice（PATH 或 SPICEUNION_NGSPICE）。

    PYTHONPATH=build/python/bindings/python python3 \
        bindings/python/examples/metrics_only.py
"""

import spiceunion as su

import os
import shutil


def main() -> None:
    if not (
        os.environ.get("SPICEUNION_NGSPICE")
        or shutil.which("ngspice")
        or shutil.which("ngspice_con")
    ):
        print("skip: ngspice executable is not available in PATH")
        return

    with su.Simulation(
        netlist_path="ngspice_builtin.cir",
        simulator="ngspice",
        ngspice_task="rc_tran",
        result_format="nspice_wrdata",
        workers=2,
        work_dir_base="local/runtime/python_metrics_only",
        metrics=[{"kind": "tran", "signal": "v(out)", "derived": ["settling_time"]}],
    ) as simulation:
        simulation.add_parameter("resistance_ohm", default_value=1000.0)
        simulation.add_parameter("capacitance_f")
        results = simulation.run([{"capacitance_f": 1e-12}, {"capacitance_f": 2e-12}])

        for index, result in enumerate(results):
            settling = result.metric("settling_time_s")
            print(f"case{index}: ok={result.ok()} metrics_ok={result.metrics_ok()}")
            print(f"  settling_time_s = {settling}")
            if not result.metrics_ok():
                print(f"  metrics_message = {result.metrics_message()}")
            # 方案一只改变提取时机：产物仍保留，read_* 仍可用。
            print(f"  read_tran -> {result.read_tran('v(out)').status_text()}")


if __name__ == "__main__":
    main()
