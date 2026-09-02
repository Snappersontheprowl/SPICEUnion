import argparse

import spiceunion as su


def main() -> None:
    parser = argparse.ArgumentParser(description="Run a minimal SPICEUnion workflow batch.")
    parser.add_argument("netlist_path")
    parser.add_argument("--simulator", default="spectre", choices=["spectre", "ngspice"])
    parser.add_argument("--workers", type=int, default=1)
    parser.add_argument("--signal", default="out")
    parser.add_argument("--execute", action="store_true")
    args = parser.parse_args()

    with su.Simulation(
        netlist_path=args.netlist_path,
        simulator=args.simulator,
        workers=args.workers,
    ) as simulation:
        simulation.add_parameter("wp", default_value=14e-6)
        simulation.add_parameter("wn", default_value=10e-6)

        cases = [{"wp": 14e-6, "wn": 10e-6}] if args.execute else []
        results = simulation.run(cases)

        for result in results:
            if not result.ok():
                print(result.status_text(), result.message)
                continue

            ac = result.read_ac(args.signal)
            if ac.ok():
                print(ac.frequency_hz, ac.real, ac.imag)
            else:
                print(ac.status_text(), ac.message)


if __name__ == "__main__":
    main()
