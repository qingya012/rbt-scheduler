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

# nlohmann/json (header-only JSON library)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

# Avoid overriding parent project settings on MSVC; harmless elsewhere.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest nlohmann_json)

add_library(rbt_scheduler_lib
  src/Scheduler.cpp
)
target_include_directories(rbt_scheduler_lib PUBLIC include)
target_compile_options(rbt_scheduler_lib PRIVATE -Wall -Wextra)
target_link_libraries(rbt_scheduler_lib
  PUBLIC
    nlohmann_json::nlohmann_json
)

enable_testing()

add_executable(rbt_scheduler_tests
  tests/scheduler_gtest.cpp
)
target_link_libraries(rbt_scheduler_tests
  PRIVATE
    rbt_scheduler_lib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(rbt_scheduler_tests)

