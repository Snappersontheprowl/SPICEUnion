import math

import spiceunion


def assert_raises_value_error(func) -> None:
    try:
        func()
    except ValueError:
        return
    raise AssertionError("expected ValueError")


def make_simulation() -> spiceunion.Simulation:
    return spiceunion.Simulation(
        netlist_path="dummy.scs",
        simulator="spectre",
        workers=1,
        work_dir_base="local/runtime/python_workflow_contract",
        timeout_seconds=1,
    )


def main() -> None:
    assert hasattr(spiceunion, "Simulation")
    assert hasattr(spiceunion, "SimulationResult")
    assert spiceunion.task_status_text(spiceunion.TaskStatus.SUCCESS) == "success"

    assert_raises_value_error(
        lambda: spiceunion.Simulation(netlist_path="", simulator="spectre")
    )
    assert_raises_value_error(
        lambda: spiceunion.Simulation(netlist_path="dummy.scs", simulator="bad")
    )
    assert_raises_value_error(
        lambda: spiceunion.Simulation(netlist_path="dummy.scs", result_format="bad")
    )
    assert_raises_value_error(
        lambda: spiceunion.Simulation(netlist_path="dummy.scs", ngspice_task="bad")
    )
    assert_raises_value_error(
        lambda: spiceunion.Simulation(netlist_path="dummy.scs", workers=0)
    )

    simulation = make_simulation()
    assert isinstance(simulation.workspace_root, str)
    assert simulation.workspace_root

    assert simulation.run([]) == []

    simulation.add_parameter("wp")
    simulation.add_parameter("wn", default_value=10e-6)
    assert_raises_value_error(lambda: simulation.add_parameter(""))
    assert_raises_value_error(lambda: simulation.add_parameter("wp"))
    assert_raises_value_error(
        lambda: simulation.add_parameter("bad_default", default_value=math.nan)
    )

    assert_raises_value_error(lambda: simulation.run([{"wn": 10e-6}]))
    assert_raises_value_error(lambda: simulation.run([{"wp": 14e-6, "extra": 1.0}]))
    assert_raises_value_error(lambda: simulation.run([{"wp": math.inf}]))

    simulation.cleanup()
    simulation.cleanup()

    with make_simulation() as scoped:
        scoped.add_parameter("wp")
        assert scoped.run([]) == []

    ngspice = spiceunion.Simulation(
        netlist_path="dummy.cir",
        simulator="ngspice",
        ngspice_task="rc_ac",
        result_format="nspice_wrdata",
    )
    ngspice.cleanup()


if __name__ == "__main__":
    main()
