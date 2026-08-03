// Manual-only probe for henjo/libpsf.
//
// This file is intentionally not part of the default CMake build. Compile it
// manually only after a local libpsf build/install is available.
//
// Example:
//
//   g++ -std=c++17 tests/manual/libpsf_probe.cpp \
//     -I/path/to/libpsf/include -L/path/to/libpsf/lib -lpsf \
//     -o /tmp/libpsf_probe
//
//   /tmp/libpsf_probe /path/to/result.raw/dcOp.dc vout
//
// Keep libpsf types out of include/su/. This probe exists only to evaluate
// whether libpsf can become an optional internal backend.

#include <algorithm>
#include <complex>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "psf.h"

namespace {

template <typename T>
void print_vector_preview(const std::vector<T>& values) {
  const std::size_t limit = std::min<std::size_t>(values.size(), 5);
  std::cout << "size=" << values.size();
  for (std::size_t i = 0; i < limit; ++i) {
    std::cout << " [" << i << "]=" << values[i];
  }
  std::cout << '\n';
}

void print_signal_preview(PSFBase* signal) {
  std::unique_ptr<PSFBase> owned(signal);

  if (auto* scalar = dynamic_cast<PSFScalar*>(owned.get())) {
    std::cout << "signal_type=scalar value=" << scalar->tostring() << '\n';
    return;
  }
  if (auto* vec = dynamic_cast<PSFDoubleVector*>(owned.get())) {
    std::cout << "signal_type=double_vector ";
    print_vector_preview(*vec);
    return;
  }
  if (auto* vec = dynamic_cast<PSFComplexDoubleVector*>(owned.get())) {
    std::cout << "signal_type=complex_double_vector ";
    print_vector_preview(*vec);
    return;
  }
  if (auto* vec = dynamic_cast<PSFInt32Vector*>(owned.get())) {
    std::cout << "signal_type=int32_vector ";
    print_vector_preview(*vec);
    return;
  }
  if (auto* vec = dynamic_cast<PSFStringVector*>(owned.get())) {
    std::cout << "signal_type=string_vector ";
    print_vector_preview(*vec);
    return;
  }

  std::cout << "signal_type=unhandled_or_null\n";
}

void print_sweep_preview(PSFVector* sweep) {
  std::unique_ptr<PSFVector> owned(sweep);
  if (!owned) {
    std::cout << "sweep_values=null\n";
    return;
  }
  if (auto* vec = dynamic_cast<PSFDoubleVector*>(owned.get())) {
    std::cout << "sweep_type=double_vector ";
    print_vector_preview(*vec);
    return;
  }
  std::cout << "sweep_type=unhandled size=" << owned->size() << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "usage: libpsf_probe <psf-file> [signal-name]\n";
    return 2;
  }

  const std::string psf_file = argv[1];
  const std::string requested_signal = argc == 3 ? argv[2] : std::string{};

  try {
    PSFDataSet dataset(psf_file);

    std::cout << "file=" << psf_file << '\n';
    std::cout << "is_swept=" << dataset.is_swept() << '\n';
    std::cout << "nsweeps=" << dataset.get_nsweeps() << '\n';
    std::cout << "sweep_npoints=" << dataset.get_sweep_npoints() << '\n';

    const auto sweep_names = dataset.get_sweep_param_names();
    std::cout << "sweep_param_names:";
    for (const auto& name : sweep_names) {
      std::cout << ' ' << name;
    }
    std::cout << '\n';

    print_sweep_preview(dataset.get_sweep_values());

    const auto signal_names = dataset.get_signal_names();
    std::cout << "signal_count=" << signal_names.size() << '\n';
    const std::size_t limit = std::min<std::size_t>(signal_names.size(), 20);
    for (std::size_t i = 0; i < limit; ++i) {
      std::cout << "signal[" << i << "]=" << signal_names[i] << '\n';
    }

    if (!requested_signal.empty()) {
      std::cout << "requested_signal=" << requested_signal << '\n';
      print_signal_preview(dataset.get_signal(requested_signal));
    }
  } catch (const std::exception& error) {
    std::cerr << "std_exception=" << error.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "unknown_exception\n";
    return 1;
  }

  return 0;
}
