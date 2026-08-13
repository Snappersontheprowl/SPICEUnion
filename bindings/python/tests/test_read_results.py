from pathlib import Path
import sys

import spiceunion


def assert_unsupported_when_libpsf_disabled(result) -> None:
    assert not result.ok()
    assert result.status == spiceunion.ResultStatus.UNSUPPORTED_FORMAT
    assert result.status_text() == "unsupported_format"


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_read_results.py <fixture-root>")

    fixture_root = Path(sys.argv[1])
    psf_root = fixture_root / "psf"

    if not spiceunion.libpsf_reader_enabled():
        assert_unsupported_when_libpsf_disabled(
            spiceunion.read_dc_value(str(psf_root / "dc_op_minimal.raw"), "vout")
        )
        assert_unsupported_when_libpsf_disabled(
            spiceunion.read_dc_sweep(
                str(psf_root / "spectre_resistor_divider_dc.raw"), "vin_dc", "out"
            )
        )
        assert_unsupported_when_libpsf_disabled(
            spiceunion.read_ac_response(str(psf_root / "spectre_rc_lowpass_ac.raw"), "out")
        )
        assert_unsupported_when_libpsf_disabled(
            spiceunion.read_tran_waveform(
                str(psf_root / "spectre_rc_charging_tran.raw"), "out", "tran.tran.tran"
            )
        )
        return

    scalar = spiceunion.read_dc_value(str(psf_root / "dc_op_minimal.raw"), "vout")
    assert scalar.ok(), scalar.message
    assert scalar.signal == "vout"
    assert scalar.value == 2.5

    sweep = spiceunion.read_dc_sweep(
        str(psf_root / "spectre_resistor_divider_dc.raw"), "vin_dc", "out"
    )
    assert sweep.ok(), sweep.message
    assert sweep.sweep_name == "vin_dc"
    assert sweep.signal == "out"
    assert len(sweep) == 11
    assert sweep.shape_consistent()
    assert abs(sweep.sweep_values[1] - 0.1) < 1.0e-15
    assert abs(sweep.values[1] - 0.025) < 1.0e-15

    ac = spiceunion.read_ac_response(str(psf_root / "spectre_rc_lowpass_ac.raw"), "out")
    assert ac.ok(), ac.message
    assert ac.signal == "out"
    assert len(ac) > 10
    assert ac.shape_consistent()

    view = spiceunion.derive_ac_view(ac)
    assert view.ok(), view.message
    assert len(view) == len(ac)
    assert view.shape_consistent()

    tran = spiceunion.read_tran_waveform(
        str(psf_root / "spectre_rc_charging_tran.raw"), "out", "tran.tran.tran"
    )
    assert tran.ok(), tran.message
    assert tran.signal == "out"
    assert len(tran) > 10
    assert tran.shape_consistent()

    settling = spiceunion.calculate_settling_time(tran, 1.0)
    assert settling.ok(), settling.message
    assert settling.settling_time_s >= 0.0

    missing = spiceunion.read_dc_sweep(
        str(psf_root / "spectre_resistor_divider_dc.raw"), "vin_dc", "missing_signal"
    )
    assert not missing.ok()
    assert missing.status == spiceunion.ResultStatus.SIGNAL_NOT_FOUND
    assert missing.status_text() == "signal_not_found"


if __name__ == "__main__":
    main()
