from pathlib import Path
import sys

import spiceunion


def require_ok(name: str, result) -> bool:
    if result.ok():
        return True
    print(f"{name}: skipped ({result.status_text()} {result.message})")
    return False


def analyze_dc_sweep(psf_root: Path) -> None:
    sweep = spiceunion.read_dc_sweep(
        str(psf_root / "spectre_resistor_divider_dc.raw"), "vin_dc", "out"
    )
    if not require_ok("dc sweep", sweep):
        return

    if not sweep.shape_consistent() or len(sweep) == 0:
        print("dc sweep: invalid shape")
        return

    first_input = sweep.sweep_values[0]
    first_output = sweep.values[0]
    last_input = sweep.sweep_values[-1]
    last_output = sweep.values[-1]
    ratio = last_output / last_input if last_input != 0.0 else 0.0

    print("dc sweep:")
    print(f"  points = {len(sweep)}")
    print(f"  first = ({first_input:g}, {first_output:g})")
    print(f"  last = ({last_input:g}, {last_output:g})")
    print(f"  last output/input ratio = {ratio:g}")


def analyze_ac_response(psf_root: Path) -> None:
    response = spiceunion.read_ac_response(str(psf_root / "spectre_rc_lowpass_ac.raw"), "out")
    if not require_ok("ac response", response):
        return

    view = spiceunion.derive_ac_view(response)
    if not require_ok("ac derived view", view):
        return

    if not view.shape_consistent() or len(view) == 0:
        print("ac derived view: invalid shape")
        return

    max_index = max(range(len(view)), key=lambda index: view.magnitude_db[index])
    min_index = min(range(len(view)), key=lambda index: view.magnitude_db[index])

    print("ac response:")
    print(f"  points = {len(view)}")
    print(
        "  max magnitude = "
        f"{view.magnitude_db[max_index]:g} dB @ {view.frequency_hz[max_index]:g} Hz"
    )
    print(
        "  min magnitude = "
        f"{view.magnitude_db[min_index]:g} dB @ {view.frequency_hz[min_index]:g} Hz"
    )


def analyze_tran_waveform(psf_root: Path) -> None:
    waveform = spiceunion.read_tran_waveform(
        str(psf_root / "spectre_rc_charging_tran.raw"), "out", "tran.tran.tran"
    )
    if not require_ok("tran waveform", waveform):
        return

    if not waveform.shape_consistent() or len(waveform) == 0:
        print("tran waveform: invalid shape")
        return

    settling = spiceunion.calculate_settling_time(waveform, 1.0)
    if not require_ok("settling time", settling):
        return

    print("tran waveform:")
    print(f"  points = {len(waveform)}")
    print(f"  start = ({waveform.time_s[0]:g}, {waveform.value[0]:g})")
    print(f"  end = ({waveform.time_s[-1]:g}, {waveform.value[-1]:g})")
    print(f"  settling_time_s = {settling.settling_time_s:g}")


def demonstrate_failure(psf_root: Path) -> None:
    missing = spiceunion.read_dc_sweep(
        str(psf_root / "spectre_resistor_divider_dc.raw"), "vin_dc", "missing_signal"
    )
    print("failure case:")
    print(f"  ok = {missing.ok()}")
    print(f"  status = {missing.status_text()}")
    print(f"  message = {missing.message}")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: analyze_fixture_results.py <fixture-root>")

    fixture_root = Path(sys.argv[1])
    psf_root = fixture_root / "psf"

    print(f"SPICEUnion {spiceunion.version()}")
    print(f"libpsf reader enabled: {spiceunion.libpsf_reader_enabled()}")

    analyze_dc_sweep(psf_root)
    analyze_ac_response(psf_root)
    analyze_tran_waveform(psf_root)
    demonstrate_failure(psf_root)


if __name__ == "__main__":
    main()
