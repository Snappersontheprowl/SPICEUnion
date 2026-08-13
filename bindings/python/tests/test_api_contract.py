import spiceunion


def require_fields(obj, names) -> None:
    for name in names:
        assert hasattr(obj, name), f"{type(obj).__name__} missing field {name}"


def main() -> None:
    assert spiceunion.status_text(spiceunion.ResultStatus.OK) == "ok"
    assert (
        spiceunion.status_text(spiceunion.ResultStatus.SIGNAL_NOT_FOUND)
        == "signal_not_found"
    )

    scalar = spiceunion.ScalarResult()
    require_fields(scalar, ["status", "message", "signal", "value"])
    assert not scalar.ok()
    assert scalar.status == spiceunion.ResultStatus.INVALID_INPUT
    assert scalar.status_text() == "invalid_input"

    sweep = spiceunion.DcSweep()
    require_fields(sweep, ["status", "message", "sweep_name", "signal"])
    require_fields(sweep, ["sweep_values", "values"])
    assert len(sweep) == 0
    assert sweep.shape_consistent()
    sweep.sweep_values = [0.0, 1.0]
    sweep.values = [0.0]
    assert not sweep.shape_consistent()

    ac = spiceunion.AcResponse()
    require_fields(ac, ["status", "message", "signal", "frequency_hz", "real", "imag"])
    assert len(ac) == 0
    assert ac.shape_consistent()
    ac.frequency_hz = [1.0, 2.0]
    ac.real = [1.0, 0.0]
    ac.imag = [0.0]
    assert not ac.shape_consistent()

    view = spiceunion.AcDerivedView()
    require_fields(view, ["status", "message", "frequency_hz", "magnitude_db", "phase_deg"])
    assert len(view) == 0
    assert view.shape_consistent()
    view.frequency_hz = [1.0]
    view.magnitude_db = [0.0]
    view.phase_deg = []
    assert not view.shape_consistent()

    tran = spiceunion.TranWaveform()
    require_fields(tran, ["status", "message", "signal", "time_s", "value"])
    assert len(tran) == 0
    assert tran.shape_consistent()
    tran.time_s = [0.0, 1.0]
    tran.value = [0.0]
    assert not tran.shape_consistent()

    ugbw = spiceunion.UgbwPhaseMarginResult()
    require_fields(
        ugbw,
        ["status", "message", "unity_gain_bandwidth_hz", "phase_margin_deg"],
    )
    assert ugbw.status_text() == "invalid_input"

    settling = spiceunion.SettlingTimeResult()
    require_fields(settling, ["status", "message", "settling_time_s"])
    assert settling.status_text() == "invalid_input"


if __name__ == "__main__":
    main()
