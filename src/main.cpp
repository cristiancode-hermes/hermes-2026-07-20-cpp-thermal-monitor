#include "thermal-monitor/thermal_stats.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <print>
#include <iostream>
#include <string>
#include <thread>

// Forward declaration (defined in dashboard_app.cpp)
int run_dashboard();

int main(int argc, char** argv) {
  bool json_mode = false;
  bool help = false;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg{argv[i]};
    if (arg == "--json") json_mode = true;
    else if (arg == "--help" || arg == "-h") help = true;
  }

  if (help) {
    std::println(std::cout, "Thermal Monitor — C++23 Linux thermal zone monitor");
    std::println(std::cout, "");
    std::println(std::cout, "  {{}} --help      Show this help");
    std::println(std::cout, "  {{}} --json      Output JSON to stdout and exit");
    std::println(std::cout, "  (no args)        Interactive TUI (FTXUI)");
    return 0;
  }

  if (json_mode) {
    auto stats = thermal::ThermalStats::collect();
    if (!stats) {
      std::println(std::cerr, "Error: {}", stats.error().message);
      return 1;
    }
    std::println(std::cout, "{}", stats->to_json().dump(2));
    return 0;
  }

  return run_dashboard();
}
