#include "thermal-monitor/thermal_stats.hpp"

#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <charconv>
#include <system_error>

namespace thermal {
namespace fs = std::filesystem;

// ── Helpers ──

Result<std::string> read_sysfs_file(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    return std::unexpected(Error{1, "Cannot open: " + path});
  }
  std::string line;
  if (!std::getline(f, line)) {
    return std::unexpected(Error{2, "Empty file: " + path});
  }
  // Trim trailing whitespace
  while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\n')) {
    line.pop_back();
  }
  return line;
}

static double parse_millicelsius(const std::string& s) {
  long long mC = 0;
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), mC);
  if (ec != std::errc{}) return 0.0;
  return static_cast<double>(mC) / 1000.0;
}

// ── Thermal Zones ──

Result<std::vector<ThermalZone>> read_thermal_zones() {
  std::vector<ThermalZone> zones;
  const std::string base = "/sys/class/thermal/";

  if (!fs::is_directory(base)) {
    return std::unexpected(Error{3, "/sys/class/thermal/ not found (not Linux?)"});
  }

  for (int i = 0; i < 128; ++i) {
    std::string dir = base + "thermal_zone" + std::to_string(i);
    if (!fs::is_directory(dir)) continue;

    ThermalZone z;
    z.index = i;

    auto type_r = read_thermal_type(i);
    z.type = type_r ? *type_r : "unknown";

    auto policy_r = read_thermal_policy(i);
    z.policy = policy_r ? *policy_r : "unknown";

    auto temp_r = read_temp_celsius(i);
    z.temp_celsius = temp_r ? *temp_r : 0.0;

    zones.push_back(std::move(z));
  }
  return zones;
}

Result<double> read_temp_celsius(int zone_index) {
  std::string path = "/sys/class/thermal/thermal_zone" + std::to_string(zone_index) + "/temp";
  auto r = read_sysfs_file(path);
  if (!r) return std::unexpected(r.error());
  return parse_millicelsius(*r);
}

Result<std::string> read_thermal_type(int zone_index) {
  std::string path = "/sys/class/thermal/thermal_zone" + std::to_string(zone_index) + "/type";
  return read_sysfs_file(path);
}

Result<std::string> read_thermal_policy(int zone_index) {
  std::string path = "/sys/class/thermal/thermal_zone" + std::to_string(zone_index) + "/policy";
  return read_sysfs_file(path);
}

// ── Cooling Devices ──

Result<std::vector<CoolingDevice>> read_cooling_devices() {
  std::vector<CoolingDevice> devices;
  const std::string base = "/sys/class/thermal/";

  if (!fs::is_directory(base)) {
    return std::unexpected(Error{3, "/sys/class/thermal/ not found"});
  }

  for (int i = 0; i < 128; ++i) {
    std::string dir = base + "cooling_device" + std::to_string(i);
    if (!fs::is_directory(dir)) continue;

    CoolingDevice dev;
    dev.index = i;

    auto type_r = read_sysfs_file(dir + "/type");
    dev.type = type_r ? *type_r : "unknown";

    auto max_r = read_sysfs_file(dir + "/max_state");
    if (max_r) {
      std::from_chars(max_r->data(), max_r->data() + max_r->size(), dev.max_state);
    } else {
      dev.max_state = 0;
    }

    auto cur_r = read_sysfs_file(dir + "/cur_state");
    if (cur_r) {
      std::from_chars(cur_r->data(), cur_r->data() + cur_r->size(), dev.cur_state);
    } else {
      dev.cur_state = 0;
    }

    dev.throttle_pct = (dev.max_state > 0)
      ? (static_cast<double>(dev.cur_state) / dev.max_state * 100.0)
      : 0.0;

    devices.push_back(std::move(dev));
  }
  return devices;
}

// ── Governors ──

Result<std::pair<std::string, std::vector<std::string>>> read_governors() {
  std::pair<std::string, std::vector<std::string>> result;

  // Read current governor from CPU0
  auto cur = read_sysfs_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
  result.first = cur ? *cur : "unknown";

  // Read available governors
  auto avail = read_sysfs_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors");
  if (avail) {
    std::istringstream ss(*avail);
    std::string gov;
    while (ss >> gov) {
      result.second.push_back(std::move(gov));
    }
  }

  return result;
}

// ── ThermalStats ──

json ThermalStats::to_json() const {
  json j;
  j["zones"] = json::array();
  for (const auto& z : zones) {
    j["zones"].push_back({
      {"index", z.index},
      {"type", z.type},
      {"temp_celsius", z.temp_celsius},
      {"policy", z.policy}
    });
  }
  j["cooling_devices"] = json::array();
  for (const auto& d : cooling_devices) {
    j["cooling_devices"].push_back({
      {"index", d.index},
      {"type", d.type},
      {"max_state", d.max_state},
      {"cur_state", d.cur_state},
      {"throttle_pct", d.throttle_pct}
    });
  }
  j["governors"] = governors;
  j["current_governor"] = current_governor;
  return j;
}

Result<ThermalStats> ThermalStats::collect() {
  ThermalStats stats;

  auto zones = read_thermal_zones();
  if (!zones) return std::unexpected(zones.error());
  stats.zones = std::move(*zones);

  auto devices = read_cooling_devices();
  if (!devices) return std::unexpected(devices.error());
  stats.cooling_devices = std::move(*devices);

  auto gov = read_governors();
  if (gov) {
    stats.current_governor = std::move(gov->first);
    stats.governors = std::move(gov->second);
  }

  return stats;
}

// ── Formatting ──

std::string fmt_temp(double celsius) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%5.1f°C", celsius);
  return buf;
}

std::string fmt_throttle(double pct) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%5.1f%%", pct);
  return buf;
}

} // namespace thermal
