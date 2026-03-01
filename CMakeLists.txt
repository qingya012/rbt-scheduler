cmake_minimum_required(VERSION 3.20)

project(rbt_scheduler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# GoogleTest (fetched if not already available)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)

# nlohmann/json — prefer the vendored single-header; fall back to FetchContent
set(NLOHMANN_VENDORED "${CMAKE_SOURCE_DIR}/cpp/third_party/nlohmann/json.hpp")
if(EXISTS "${NLOHMANN_VENDORED}")
  message(STATUS "nlohmann/json: using vendored copy at cpp/third_party/")
  add_library(nlohmann_json INTERFACE)
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
  target_include_directories(nlohmann_json INTERFACE
    "${CMAKE_SOURCE_DIR}/cpp/third_party"
  )
else()
  message(STATUS "nlohmann/json: vendored copy not found, fetching via FetchContent")
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

# Avoid overriding parent project settings on MSVC; harmless elsewhere.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

add_library(rbt_scheduler_lib
  cpp/src/Scheduler.cpp
)
target_include_directories(rbt_scheduler_lib PUBLIC cpp/include)
target_compile_options(rbt_scheduler_lib PRIVATE -Wall -Wextra)
target_link_libraries(rbt_scheduler_lib
  PUBLIC
    nlohmann_json::nlohmann_json
)

# Main scheduler executable (optional; links to the same library)
add_executable(scheduler cpp/main.cpp)
target_link_libraries(scheduler PRIVATE rbt_scheduler_lib)

enable_testing()

add_executable(rbt_scheduler_tests
  cpp/tests/scheduler_gtest.cpp
)
target_link_libraries(rbt_scheduler_tests
  PRIVATE
    rbt_scheduler_lib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(rbt_scheduler_tests)

