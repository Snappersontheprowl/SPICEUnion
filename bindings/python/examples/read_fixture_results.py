from pathlib import Path
import sys

import spiceunion


def describe_result(name: str, result) -> None:
    if result.ok():
        print(f"{name}: ok")
    else:
        print(f"{name}: {result.status_text()} {result.message}")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: read_fixture_results.py <fixture-root>")

    fixture_root = Path(sys.argv[1])
    psf_root = fixture_root / "psf"

    print(f"SPICEUnion {spiceunion.version()}")
    print(f"libpsf reader enabled: {spiceunion.libpsf_reader_enabled()}")

    scalar = spiceunion.read_dc_value(str(psf_root / "dc_op_minimal.raw"), "vout")
    describe_result("dc scalar", scalar)
    if scalar.ok():
        print(f"  {scalar.signal} = {scalar.value:g}")

    sweep = spiceunion.read_dc_sweep(
        str(psf_root / "spectre_resistor_divider_dc.raw"), "vin_dc", "out"
    )
    describe_result("dc sweep", sweep)
    if sweep.ok():
        print(f"  points = {len(sweep)}")
        print(f"  shape consistent = {sweep.shape_consistent()}")

    ac = spiceunion.read_ac_response(str(psf_root / "spectre_rc_lowpass_ac.raw"), "out")
    describe_result("ac response", ac)
    if ac.ok():
        view = spiceunion.derive_ac_view(ac)
        describe_result("ac derived view", view)
        if view.ok():
            print(f"  points = {len(view)}")

    tran = spiceunion.read_tran_waveform(
        str(psf_root / "spectre_rc_charging_tran.raw"), "out", "tran.tran.tran"
    )
    describe_result("tran waveform", tran)
    if tran.ok():
        settling = spiceunion.calculate_settling_time(tran, 1.0)
        describe_result("settling time", settling)
        if settling.ok():
            print(f"  settling_time_s = {settling.settling_time_s:g}")


if __name__ == "__main__":
    main()
