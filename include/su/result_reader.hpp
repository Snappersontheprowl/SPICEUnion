#pragma once

#include "su/result.hpp"

#include <string>
#include <vector>

namespace su {

ReadResult<ResultDirectory> find_result_directory(const std::string& work_dir);

ReadResult<ScalarResult> read_dc_value(const std::string& result_dir,
                                       const std::string& signal_name,
                                       ResultFormat format = ResultFormat::kUnknown);

ReadResult<DcSweep> read_dc_sweep(const std::string& result_dir,
                                  const std::string& sweep_name,
                                  const std::string& signal_name,
                                  const std::string& filename = "dc.dc",
                                  ResultFormat format = ResultFormat::kUnknown);

ReadResult<AcResponse> read_ac_response(const std::string& result_dir,
                                        const std::string& signal_name,
                                        const std::string& filename = "ac.ac",
                                        ResultFormat format = ResultFormat::kUnknown);

ReadResult<TranWaveform> read_tran_waveform(const std::string& result_dir,
                                            const std::string& signal_name,
                                            const std::string& filename = "tran.tran",
                                            ResultFormat format = ResultFormat::kUnknown);

ReadResult<std::vector<SensitivityEntry>> read_sensitivity_legacy(const std::string& work_dir);

ReadResult<AcDerivedView> derive_ac_view(const AcResponse& response);

ReadResult<UgbwPhaseMarginResult> calculate_ugbw_and_phase_margin(const AcDerivedView& response);

ReadResult<SettlingTimeResult> calculate_settling_time(const TranWaveform& waveform,
                                                       double target_value,
                                                       double error_band = 0.01);

}  // namespace su
