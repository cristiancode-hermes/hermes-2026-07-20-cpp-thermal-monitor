cmake_minimum_required(VERSION 3.28)
project(cpp-thermal-monitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──
include(FetchContent)

# nlohmann_json (header-only JSON)
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog (header-only logging)
FetchContent_Declare(spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.15.1
)
FetchContent_MakeAvailable(spdlog)

# FTXUI (terminal UI library)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v5.0.0
  SYSTEM
)
FetchContent_MakeAvailable(ftxui)

# ── Sources (library: everything except main.cpp) ──
file(GLOB_RECURSE LIB_SRCS src/*.cpp)
list(FILTER LIB_SRCS EXCLUDE REGEX "main\\.cpp$")
file(GLOB_RECURSE HEADERS include/*.hpp include/*.h)

add_library(thermal-monitor-lib STATIC ${LIB_SRCS} ${HEADERS})
target_include_directories(thermal-monitor-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(thermal-monitor-lib PUBLIC
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  ftxui::screen
  ftxui::dom
  ftxui::component
  pthread
)

# ── Executable ──
add_executable(thermal-monitor src/main.cpp)
target_link_libraries(thermal-monitor PRIVATE thermal-monitor-lib)

# ── Tests ──
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ── Install ──
install(TARGETS thermal-monitor DESTINATION bin)
