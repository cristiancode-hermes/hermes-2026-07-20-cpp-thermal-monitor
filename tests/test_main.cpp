#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

#include "thermal-monitor/thermal_stats.hpp"
#include <filesystem>
#include <fstream>

using namespace std::string_literals;

namespace fs = std::filesystem;

// ── Helper: check if we're on Linux with thermal sysfs ──
static bool has_thermal_sysfs() {
  return fs::is_directory("/sys/class/thermal");
}

// ── Unit: Temperature parsing ──
TEST_CASE("Temperature millicelsius parsing", "[unit][core]") {
  // read_sysfs_file returns raw content; a thermal zone temp file
  // usually contains e.g. "45000\n" → 45.0°C
  // We test via the actual reader if available

  SECTION("Read CPU thermal zone type exists on Linux") {
    if (!has_thermal_sysfs()) {
      SUCCEED("Skipping: not on Linux or no thermal sysfs");
      return;
    }
    for (int i = 0; i < 8; ++i) {
      std::string path = "/sys/class/thermal/thermal_zone"s + std::to_string(i) + "/type";
      if (!fs::exists(path)) continue;
      auto r = thermal::read_sysfs_file(path);
      REQUIRE(r.has_value());
      REQUIRE(!r->empty());
    }
  }

  SECTION("Read temperature from thermal zones") {
    if (!has_thermal_sysfs()) {
      SUCCEED("Skipping: not on Linux or no thermal sysfs");
      return;
    }
    auto zones = thermal::read_thermal_zones();
    REQUIRE(zones.has_value());
    if (!zones->empty()) {
      // Temperatures on Linux are typically between 20°C and 100°C
      for (const auto& z : *zones) {
        CAPTURE(z.index, z.type, z.temp_celsius);
        if (z.temp_celsius > 0) {
          REQUIRE(z.temp_celsius > 15.0);  // above room temp
          REQUIRE(z.temp_celsius < 120.0); // below critical
        }
      }
    }
  }
}

// ── Unit: Cooling devices ──
TEST_CASE("Cooling device reading", "[unit][cooling]") {
  if (!has_thermal_sysfs()) {
    SUCCEED("Skipping: not on Linux or no thermal sysfs");
    return;
  }

  auto devices = thermal::read_cooling_devices();
  REQUIRE(devices.has_value());

  for (const auto& d : *devices) {
    CAPTURE(d.index, d.type, d.max_state, d.cur_state);
    REQUIRE(!d.type.empty());
    REQUIRE(d.max_state >= 0);
    REQUIRE(d.cur_state >= 0);
    REQUIRE(d.cur_state <= d.max_state);
    if (d.max_state > 0) {
      REQUIRE(d.throttle_pct >= 0.0);
      REQUIRE(d.throttle_pct <= 100.0);
    }
  }
}

// ── Unit: Governors ──
TEST_CASE("CPU governors reading", "[unit][governors]") {
  std::string path = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors";
  if (!fs::exists(path)) {
    SUCCEED("Skipping: cpufreq not available");
    return;
  }

  auto gov = thermal::read_governors();
  REQUIRE(gov.has_value());
  REQUIRE(!gov->first.empty());      // current governor
  REQUIRE(!gov->second.empty());     // available governors

  // Common governors
  bool has_performance = false;
  for (const auto& g : gov->second) {
    if (g == "performance") has_performance = true;
  }
  // Not requiring performance — could be powersave, schedutil, etc.
  CAPTURE(gov->first);
  CAPTURE(gov->second);
}

// ── Unit: JSON serialization ──
TEST_CASE("ThermalStats JSON output", "[unit][json]") {
  auto stats = thermal::ThermalStats::collect();
  REQUIRE(stats.has_value());

  thermal::json j = stats->to_json();
  REQUIRE(j.contains("zones"));
  REQUIRE(j.contains("cooling_devices"));
  REQUIRE(j.contains("governors"));
  REQUIRE(j.contains("current_governor"));

  // Verify zone structure
  if (!j["zones"].empty()) {
    auto z = j["zones"][0];
    REQUIRE(z.contains("index"));
    REQUIRE(z.contains("type"));
    REQUIRE(z.contains("temp_celsius"));
    REQUIRE(z.contains("policy"));
    // temp must be numeric
    REQUIRE(z["temp_celsius"].is_number());
  }

  // Verify cooling device structure
  if (!j["cooling_devices"].empty()) {
    auto d = j["cooling_devices"][0];
    REQUIRE(d.contains("index"));
    REQUIRE(d.contains("type"));
    REQUIRE(d.contains("max_state"));
    REQUIRE(d.contains("cur_state"));
    REQUIRE(d.contains("throttle_pct"));
    REQUIRE(d["throttle_pct"].is_number());
  }
}

// ── Unit: Error handling for invalid paths ──
TEST_CASE("Read nonexistent file returns error", "[unit][error]") {
  auto r = thermal::read_sysfs_file("/sys/class/thermal/nonexistent_file_xyz");
  REQUIRE(!r.has_value());
  REQUIRE(r.error().code > 0);
  REQUIRE(!r.error().message.empty());
}

// ── Unit: Formatting helpers ──
TEST_CASE("Temperature formatting", "[unit][format]") {
  std::string f = thermal::fmt_temp(45.0);
  REQUIRE(f.find("45") != std::string::npos);
  REQUIRE(f.find("°") != std::string::npos);
}

TEST_CASE("Throttle percentage formatting", "[unit][format]") {
  std::string f = thermal::fmt_throttle(75.5);
  REQUIRE(f.find("75.5") != std::string::npos);
  REQUIRE(f.find("%") != std::string::npos);
}

// ── Benchmark: reading thermal stats ──
TEST_CASE("Thermal stats collection benchmark", "[benchmark]") {
  BENCHMARK("Collect all thermal stats") {
    auto r = thermal::ThermalStats::collect();
    return r.has_value() ? r->zones.size() : 0;
  };
}
