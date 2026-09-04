#include "su/ngspice_session.hpp"

#include <gtest/gtest.h>

TEST(NgspiceRcAcConfigTest, UsesDefaultsAndStateOverrides) {
  const auto defaults = su::ngspice_rc_ac_config_from_state({});
  EXPECT_DOUBLE_EQ(defaults.resistance_ohm, 1000.0);
  EXPECT_DOUBLE_EQ(defaults.capacitance_f, 1.0e-12);
  EXPECT_DOUBLE_EQ(defaults.start_hz, 1.0e6);
  EXPECT_DOUBLE_EQ(defaults.stop_hz, 1.0e10);
  EXPECT_EQ(defaults.points_per_decade, 20);

  const su::ParameterState state{
      {"resistance_ohm", 2000.0}, {"capacitance_f", 2.0e-12},  {"ac_start_hz", 10.0},
      {"ac_stop_hz", 1.0e9},      {"points_per_decade", 33.0},
  };

  const auto config = su::ngspice_rc_ac_config_from_state(state);
  EXPECT_DOUBLE_EQ(config.resistance_ohm, 2000.0);
  EXPECT_DOUBLE_EQ(config.capacitance_f, 2.0e-12);
  EXPECT_DOUBLE_EQ(config.start_hz, 10.0);
  EXPECT_DOUBLE_EQ(config.stop_hz, 1.0e9);
  EXPECT_EQ(config.points_per_decade, 33);
}

TEST(NgspiceRcTranConfigTest, UsesDefaultsAndStateOverrides) {
  const auto defaults = su::ngspice_rc_tran_config_from_state({});
  EXPECT_DOUBLE_EQ(defaults.resistance_ohm, 1000.0);
  EXPECT_DOUBLE_EQ(defaults.capacitance_f, 1.0e-12);
  EXPECT_DOUBLE_EQ(defaults.input_voltage_v, 1.0);
  EXPECT_DOUBLE_EQ(defaults.step_s, 1.0e-11);
  EXPECT_DOUBLE_EQ(defaults.stop_s, 1.0e-8);

  const su::ParameterState state{
      {"resistance_ohm", 2000.0}, {"capacitance_f", 2.0e-12}, {"input_voltage_v", 1.2},
      {"tran_step_s", 2.0e-11},   {"tran_stop_s", 2.0e-8},
  };

  const auto config = su::ngspice_rc_tran_config_from_state(state);
  EXPECT_DOUBLE_EQ(config.resistance_ohm, 2000.0);
  EXPECT_DOUBLE_EQ(config.capacitance_f, 2.0e-12);
  EXPECT_DOUBLE_EQ(config.input_voltage_v, 1.2);
  EXPECT_DOUBLE_EQ(config.step_s, 2.0e-11);
  EXPECT_DOUBLE_EQ(config.stop_s, 2.0e-8);
}

TEST(NgspiceResistorDividerDcConfigTest, UsesDefaultsAndStateOverrides) {
  const auto defaults = su::ngspice_resistor_divider_dc_config_from_state({});
  EXPECT_DOUBLE_EQ(defaults.top_resistance_ohm, 1000.0);
  EXPECT_DOUBLE_EQ(defaults.bottom_resistance_ohm, 1000.0);
  EXPECT_DOUBLE_EQ(defaults.sweep_start_v, 0.0);
  EXPECT_DOUBLE_EQ(defaults.sweep_stop_v, 1.0);
  EXPECT_DOUBLE_EQ(defaults.sweep_step_v, 0.1);

  const su::ParameterState state{
      {"top_resistance_ohm", 3000.0},
      {"bottom_resistance_ohm", 1000.0},
      {"dc_start_v", -1.0},
      {"dc_stop_v", 1.0},
      {"dc_step_v", 0.25},
  };

  const auto config = su::ngspice_resistor_divider_dc_config_from_state(state);
  EXPECT_DOUBLE_EQ(config.top_resistance_ohm, 3000.0);
  EXPECT_DOUBLE_EQ(config.bottom_resistance_ohm, 1000.0);
  EXPECT_DOUBLE_EQ(config.sweep_start_v, -1.0);
  EXPECT_DOUBLE_EQ(config.sweep_stop_v, 1.0);
  EXPECT_DOUBLE_EQ(config.sweep_step_v, 0.25);
}
