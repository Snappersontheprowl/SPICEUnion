#pragma once

#include "su/result.hpp"
#include "su/result_reader.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>

namespace spiceunion_test {

inline double find_frequency_closest_to_db(const su::AcDerivedView& view, double target_db) {
  double best_frequency = 0.0;
  double best_error = 1.0e300;
  for (std::size_t index = 0; index < view.size(); ++index) {
    const double error = std::abs(view.magnitude_db[index] - target_db);
    if (error < best_error) {
      best_error = error;
      best_frequency = view.frequency_hz[index];
    }
  }
  return best_frequency;
}

inline void expect_rc_lowpass_ac_semantics(const su::AcResponse& response, double resistance_ohm,
                                           double capacitance_f) {
  ASSERT_TRUE(response.shape_consistent());
  ASSERT_GT(response.size(), 10U);

  for (std::size_t index = 1; index < response.frequency_hz.size(); ++index) {
    EXPECT_GT(response.frequency_hz[index], response.frequency_hz[index - 1]);
  }

  const auto view = su::derive_ac_view(response);
  ASSERT_TRUE(view.ok()) << view.error_message;
  ASSERT_TRUE(view.value.shape_consistent());

  EXPECT_NEAR(view.value.magnitude_db.front(), 0.0, 0.1);

  const double expected_fc = 1.0 / (2.0 * std::acos(-1.0) * resistance_ohm * capacitance_f);
  const double measured_fc = find_frequency_closest_to_db(view.value, -3.01029995664);
  EXPECT_NEAR(measured_fc, expected_fc, expected_fc * 0.03);
}

inline double interpolate_at_time(const su::TranWaveform& waveform, double target_time_s) {
  if (waveform.time_s.empty()) {
    return 0.0;
  }
  if (target_time_s <= waveform.time_s.front()) {
    return waveform.value.front();
  }

  for (std::size_t index = 1; index < waveform.time_s.size(); ++index) {
    if (target_time_s <= waveform.time_s[index]) {
      const double left_time = waveform.time_s[index - 1];
      const double right_time = waveform.time_s[index];
      const double left_value = waveform.value[index - 1];
      const double right_value = waveform.value[index];
      const double ratio = (target_time_s - left_time) / (right_time - left_time);
      return left_value + ratio * (right_value - left_value);
    }
  }

  return waveform.value.back();
}

inline void expect_rc_charging_tran_semantics(const su::TranWaveform& waveform,
                                              double resistance_ohm, double capacitance_f,
                                              double input_voltage_v) {
  ASSERT_TRUE(waveform.shape_consistent());
  ASSERT_GT(waveform.size(), 10U);

  for (std::size_t index = 1; index < waveform.time_s.size(); ++index) {
    EXPECT_GT(waveform.time_s[index], waveform.time_s[index - 1]);
    EXPECT_GE(waveform.value[index] + std::abs(input_voltage_v) * 1.0e-6,
              waveform.value[index - 1]);
  }

  const double tau = resistance_ohm * capacitance_f;
  const double expected_at_tau = input_voltage_v * (1.0 - std::exp(-1.0));
  const double expected_at_five_tau = input_voltage_v * (1.0 - std::exp(-5.0));

  EXPECT_NEAR(waveform.value.front(), 0.0, std::abs(input_voltage_v) * 0.02);
  EXPECT_NEAR(interpolate_at_time(waveform, tau), expected_at_tau,
              std::abs(input_voltage_v) * 0.03);
  EXPECT_NEAR(interpolate_at_time(waveform, 5.0 * tau), expected_at_five_tau,
              std::abs(input_voltage_v) * 0.02);
  EXPECT_NEAR(waveform.value.back(), input_voltage_v, std::abs(input_voltage_v) * 0.02);
}

}  // namespace spiceunion_test
