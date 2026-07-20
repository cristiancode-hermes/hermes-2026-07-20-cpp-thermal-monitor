#pragma once

#include <expected>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace thermal {

using json = nlohmann::json;

struct Error {
  int code;
  std::string message;
};

template <typename T>
using Result = std::expected<T, Error>;

// ── Thermal Zone ──
struct ThermalZone {
  int index;
  std::string type;         // e.g. "x86_pkg_temp", "acpitz"
  double temp_celsius;
  std::string policy;       // e.g. "step_wise", "user_space"
};

// ── Cooling Device ──
struct CoolingDevice {
  int index;
  std::string type;         // e.g. "Processor", "intel_powerclamp"
  int max_state;
  int cur_state;
  double throttle_pct;      // cur_state / max_state * 100
};

// ── Thermal Stats ──
struct ThermalStats {
  std::vector<ThermalZone> zones;
  std::vector<CoolingDevice> cooling_devices;
  std::vector<std::string> governors;       // available CPU freq governors
  std::string current_governor;

  json to_json() const;
  static Result<ThermalStats> collect();
};

// ── Readers ──
Result<std::vector<ThermalZone>> read_thermal_zones();
Result<std::vector<CoolingDevice>> read_cooling_devices();
Result<std::string> read_sysfs_file(const std::string& path);
Result<double> read_temp_celsius(int zone_index);
Result<std::string> read_thermal_type(int zone_index);
Result<std::string> read_thermal_policy(int zone_index);
Result<std::pair<std::string, std::vector<std::string>>> read_governors();

// ── Formatting ──
std::string fmt_temp(double celsius);
std::string fmt_throttle(double pct);

} // namespace thermal
