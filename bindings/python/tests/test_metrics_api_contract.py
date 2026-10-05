"""Python metrics 接口契约：参数校验、枚举与空提取语义。"""

import spiceunion as su


def assert_raises_value_error(func) -> None:
    try:
        func()
    except ValueError:
        return
    raise AssertionError("expected ValueError")


def make_simulation(metrics) -> su.Simulation:
    return su.Simulation(
        netlist_path="dummy.scs",
        simulator="ngspice",
        workers=1,
        work_dir_base="local/runtime/python_metrics_contract",
        timeout_seconds=1,
        metrics=metrics,
    )


def main() -> None:
    assert hasattr(su, "MetricKind")
    assert hasattr(su, "MetricsStatus")
    assert hasattr(su, "DerivedMetric")
    assert hasattr(su, "ResultStatus")
    assert su.ResultStatus.ARTIFACTS_NOT_RETAINED.name == "ARTIFACTS_NOT_RETAINED"

    # 合法声明（不运行仿真，只验证解析与构造）。
    make_simulation([{"kind": "tran", "signal": "v(out)", "derived": ["settling_time"]}])
    make_simulation([])
    make_simulation(None)

    assert_raises_value_error(lambda: make_simulation([{"signal": "vout", "kind": "bad"}]))
    assert_raises_value_error(lambda: make_simulation([{"kind": "dc"}]))
    assert_raises_value_error(
        lambda: make_simulation([{"kind": "ac", "signal": "out", "derived": ["bad"]}])
    )


if __name__ == "__main__":
    main()
