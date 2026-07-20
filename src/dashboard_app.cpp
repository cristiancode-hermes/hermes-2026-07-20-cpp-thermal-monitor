#include "thermal-monitor/thermal_stats.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;
using namespace std::chrono_literals;

Decorator temp_color(double celsius);
Element make_percent_bar(double pct, Decorator fg, int width = 20);

// ── Helper: color by temperature ──
Decorator temp_color(double celsius) {
  if (celsius > 80.0) return color(Color::Red) | bold;
  if (celsius > 60.0) return color(Color::Yellow) | bold;
  return color(Color::Green);
}

// ── Progress bar ──
Element make_percent_bar(double pct, Decorator fg, int width) {
  int filled = std::clamp(static_cast<int>(std::round(pct / 100.0 * width)), 0, width);
  auto fill = text(std::string(filled, '|')) | fg;
  auto empty_elem = text(std::string(width - filled, '.')) | color(Color::GrayDark);
  return hbox(Elements{std::move(fill), std::move(empty_elem)});
}

// ── Thermal monitor line ──
Element thermal_zone_line(const thermal::ThermalZone& z) {
  // Normalize temp to 0-100 scale for bar
  double pct = std::clamp(z.temp_celsius / 100.0 * 100.0, 0.0, 100.0);
  return hbox(Elements{
    text(z.type) | size(WIDTH, GREATER_THAN, 22),
    separator(),
    text(thermal::fmt_temp(z.temp_celsius)) | size(WIDTH, GREATER_THAN, 10),
    separator(),
    make_percent_bar(pct, temp_color(z.temp_celsius)),
    text("  ") | size(WIDTH, GREATER_THAN, 2),
    text(z.policy) | color(Color::GrayDark),
  });
}

// ── Cooling device line ──
Element cooling_device_line(const thermal::CoolingDevice& d) {
  return hbox(Elements{
    text(d.type) | size(WIDTH, GREATER_THAN, 22),
    separator(),
    text(thermal::fmt_throttle(d.throttle_pct)) | size(WIDTH, GREATER_THAN, 8),
    separator(),
    make_percent_bar(d.throttle_pct, color(Color::Cyan)),
    text("  ") | size(WIDTH, GREATER_THAN, 2),
    text(std::to_string(d.cur_state) + "/" + std::to_string(d.max_state)) | color(Color::GrayDark),
  });
}

// ── Dashboard ──
int run_dashboard() {
  auto screen = ScreenInteractive::Fullscreen();
  std::atomic<bool> running{true};

  // Shared state
  thermal::ThermalStats stats;

  // Renderer
  auto renderer = Renderer([&] {
    Elements zone_elements;
    zone_elements.push_back(text("Thermal Zones") | bold | color(Color::Cyan) | hcenter);
    zone_elements.push_back(separator());

    for (const auto& z : stats.zones) {
      zone_elements.push_back(thermal_zone_line(z));
    }

    if (stats.zones.empty()) {
      zone_elements.push_back(text("No thermal zones found") | color(Color::GrayDark) | hcenter);
    }

    Elements cooling_elements;
    cooling_elements.push_back(text("Cooling Devices") | bold | color(Color::Cyan) | hcenter);
    cooling_elements.push_back(separator());

    for (const auto& d : stats.cooling_devices) {
      cooling_elements.push_back(cooling_device_line(d));
    }

    if (stats.cooling_devices.empty()) {
      cooling_elements.push_back(text("No cooling devices found") | color(Color::GrayDark) | hcenter);
    }

    // Governor info
    Elements gov_elements;
    gov_elements.push_back(separator());
    gov_elements.push_back(hbox(Elements{
      text("CPU Governor: ") | bold,
      text(stats.current_governor) | color(Color::Yellow) | bold,
    }));

    return window(
      text(" Thermal Monitor ") | bold | color(Color::Cyan),
      vbox(Elements{
        window(
          text(" CPU Temperature "),
          vbox(Elements{
            vbox(std::move(zone_elements)) | border,
          })
        ),
        window(
          text(" Cooling Devices "),
          vbox(Elements{
            vbox(std::move(cooling_elements)) | border,
          })
        ),
        window(
          text(" System Info "),
          vbox(std::move(gov_elements)) | border
        ),
        separator(),
        text("  Ctrl+C to exit") | color(Color::GrayDark),
      }) | border
    );
  });

  // Refresh thread
  std::thread refresh_thread([&] {
    while (running) {
      auto result = thermal::ThermalStats::collect();
      if (result) {
        stats = std::move(*result);
      }
      screen.PostEvent(Event::Custom);
      std::this_thread::sleep_for(2000ms);
    }
  });

  screen.Loop(renderer);
  running = false;
  if (refresh_thread.joinable()) refresh_thread.join();
  return 0;
}
