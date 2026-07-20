# Thermal Monitor — Monitor Térmico de Sistema Linux en TUI

Monitor interactivo de temperaturas del sistema Linux que lee estadísticas directamente de `/sys/class/thermal/` usando **C++23** con interfaz **FTXUI** en terminal.

## Características

- **Zonas térmicas**: Temperatura por zona (CPU package, ACPI, etc.) con barras de progreso coloreadas
- **Cooling devices**: Estado de dispositivos de enfriamiento (throttle porcentual, estado actual/máximo)
- **CPU Governor**: Gobernador actual y gobernadores disponibles del escalador de frecuencia
- **Modo JSON** (`--json`): Salida estructurada para integración con scripts
- **Actualización cada 2 segundos**: Refresco automático mediante hilo en background con PostEvent
- **Barras de progreso**: Color verde (<60°C), amarillo (60-80°C), rojo (>80°C)
- Arquitectura RAII con `std::expected` para manejo de errores sin excepciones

## Requisitos

- Compilador: GCC 14+ o Clang 18+
- CMake 3.28+
- Sistema Linux con `/sys/class/thermal/` (prácticamente todos los kernels modernos)

## Compilación y ejecución

```bash
# Configurar (debug con sanitizers)
cmake --preset debug

# Compilar
cmake --build --preset debug

# Ejecutar TUI
./build/debug/thermal-monitor

# Ejecutar en modo JSON
./build/debug/thermal-monitor --json

# Ejecutar tests
cd build/debug && ctest --output-on-failure
```

## Uso con build directory corto

Para evitar problemas con rutas largas:

```bash
cmake -G "Unix Makefiles" -S . -B /tmp/thermal-monitor-build -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/thermal-monitor-build
/tmp/thermal-monitor-build/thermal-monitor
```

## Modo JSON

```bash
$ ./thermal-monitor --json
{
  "zones": [
    {
      "index": 0,
      "type": "x86_pkg_temp",
      "temp_celsius": 45.0,
      "policy": "step_wise"
    }
  ],
  "cooling_devices": [
    {
      "index": 0,
      "type": "Processor",
      "max_state": 100,
      "cur_state": 0,
      "throttle_pct": 0.0
    }
  ],
  "governors": ["powersave", "performance", "schedutil"],
  "current_governor": "powersave"
}
```

## Tecnologías

- **C++23** — `std::expected`, `std::println`, `std::from_chars`
- **FTXUI v5** — Terminal UI interactiva
- **nlohmann/json** — Serialización JSON
- **spdlog** — Logging estructurado
- **Catch2 v3** — Tests unitarios, integración y benchmarks

## Estructura

```
├── CMakeLists.txt
├── CMakePresets.json
├── include/
│   └── thermal-monitor/
│       └── thermal_stats.hpp      # API pública y tipos
├── src/
│   ├── main.cpp                   # Entry point + CLI parsing
│   ├── thermal_stats.cpp           # Lectura de /sys/class/thermal/
│   └── dashboard_app.cpp          # Interfaz FTXUI
├── tests/
│   ├── CMakeLists.txt
│   └── test_main.cpp              # Tests unitarios + benchmarks
└── README.md
```
