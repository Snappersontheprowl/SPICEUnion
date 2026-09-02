import os
from pathlib import Path
import shutil

import spiceunion as su


def external_workflow_enabled() -> bool:
    return os.environ.get("SPICEUNION_ENABLE_PYTHON_WORKFLOW_EXTERNAL_TESTS") == "1"


def ngspice_available() -> bool:
    explicit = os.environ.get("SPICEUNION_NGSPICE")
    if explicit and os.access(explicit, os.X_OK):
        return True
    return shutil.which("ngspice_con") is not None or shutil.which("ngspice") is not None


def main() -> None:
    if not external_workflow_enabled():
        print(
            "skip: set SPICEUNION_ENABLE_PYTHON_WORKFLOW_EXTERNAL_TESTS=1 "
            "to run Python workflow external smoke"
        )
        return

    if not ngspice_available():
        print("skip: ngspice executable is not available in PATH")
        return

    runtime_root = Path("local/runtime/python_workflow_external")
    shutil.rmtree(runtime_root, ignore_errors=True)

    with su.Simulation(
        netlist_path="ngspice_builtin.cir",
        simulator="ngspice",
        workers=1,
        work_dir_base=str(runtime_root),
        workspace_namespace="rc_ac",
        result_format="nspice_wrdata",
        ngspice_task="rc_ac",
    ) as simulation:
        simulation.add_parameter("resistance_ohm", default_value=1000.0)
        simulation.add_parameter("capacitance_f", default_value=1.0e-12)
        results = simulation.run([{}])

    assert len(results) == 1
    assert results[0].ok(), results[0].message
    assert results[0].status == su.TaskStatus.SUCCESS
    assert results[0].status_text() == "success"
    assert results[0].result_format == "nspice_wrdata"

    ac = results[0].read_ac("v(out)")
    assert ac.ok(), ac.message
    assert ac.signal == "v(out)"
    assert len(ac) > 0
    assert ac.shape_consistent()

    shutil.rmtree(runtime_root, ignore_errors=True)


if __name__ == "__main__":
    main()
