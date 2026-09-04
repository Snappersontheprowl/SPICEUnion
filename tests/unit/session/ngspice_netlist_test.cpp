#include "su/ngspice_session.hpp"

#include <gtest/gtest.h>

#include <string>

TEST(NgspiceRcAcNetlistTest, RendersBatchModeLowPassAcNetlist) {
  su::NgspiceRcAcConfig config;
  config.resistance_ohm = 1000.0;
  config.capacitance_f = 1.0e-12;
  config.start_hz = 1.0e6;
  config.stop_hz = 1.0e10;
  config.points_per_decade = 20;

  const auto netlist = su::render_ngspice_rc_ac_netlist(config, "rc_ac.out");

  EXPECT_NE(netlist.find("Vin in 0 dc 0 ac 1"), std::string::npos);
  EXPECT_NE(netlist.find("R1 in out 1.00000000000000000e+03"), std::string::npos);
  EXPECT_NE(netlist.find("C1 out 0 9.99999999999999980e-13"), std::string::npos);
  EXPECT_NE(netlist.find("ac dec 20 1.00000000000000000e+06 1.00000000000000000e+10"),
            std::string::npos);
  EXPECT_NE(netlist.find("wrdata rc_ac.out v(out)"), std::string::npos);
}

TEST(NgspiceRcTranNetlistTest, RendersBatchModeChargingNetlist) {
  su::NgspiceRcTranConfig config;
  config.resistance_ohm = 1000.0;
  config.capacitance_f = 1.0e-12;
  config.input_voltage_v = 1.0;
  config.step_s = 1.0e-11;
  config.stop_s = 1.0e-8;

  const auto netlist = su::render_ngspice_rc_tran_netlist(config, "rc_tran.out");

  EXPECT_NE(netlist.find("Vin in 0 dc 1.00000000000000000e+00"), std::string::npos);
  EXPECT_NE(netlist.find("R1 in out 1.00000000000000000e+03"), std::string::npos);
  EXPECT_NE(netlist.find("C1 out 0 9.99999999999999980e-13 ic=0"), std::string::npos);
  EXPECT_NE(netlist.find("tran "), std::string::npos);
  EXPECT_NE(netlist.find(" uic"), std::string::npos);
  EXPECT_NE(netlist.find("wrdata rc_tran.out v(out)"), std::string::npos);
}

TEST(NgspiceResistorDividerDcNetlistTest, RendersBatchModeDcSweepNetlist) {
  su::NgspiceResistorDividerDcConfig config;
  config.top_resistance_ohm = 3000.0;
  config.bottom_resistance_ohm = 1000.0;
  config.sweep_start_v = -1.0;
  config.sweep_stop_v = 1.0;
  config.sweep_step_v = 0.25;

  const auto netlist = su::render_ngspice_resistor_divider_dc_netlist(config, "dc.out");

  EXPECT_NE(netlist.find("Vin in 0 dc 0"), std::string::npos);
  EXPECT_NE(netlist.find("Rtop in out 3.00000000000000000e+03"), std::string::npos);
  EXPECT_NE(netlist.find("Rbottom out 0 1.00000000000000000e+03"), std::string::npos);
  EXPECT_NE(netlist.find("dc Vin -1.00000000000000000e+00 1.00000000000000000e+00 "
                         "2.50000000000000000e-01"),
            std::string::npos);
  EXPECT_NE(netlist.find("wrdata dc.out v(out)"), std::string::npos);
}
