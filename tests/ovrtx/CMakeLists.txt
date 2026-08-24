# Standalone USD export / camera tests. No Qt, OpenCascade, or NVIDIA GPU.
#   cmake -S tests/ovrtx -B build-ovrtx-usd
#   cmake --build build-ovrtx-usd
#   ctest --test-dir build-ovrtx-usd --output-on-failure

cmake_minimum_required(VERSION 3.16)
project(test_ovrtx_usd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MayoOvrtx_Root ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(
    test_ovrtx_usd
    ${MayoOvrtx_Root}/src/ovrtx/ovrtx_usd_types.cpp
    ${MayoOvrtx_Root}/src/ovrtx/ovrtx_camera.cpp
    ${MayoOvrtx_Root}/src/ovrtx/ovrtx_usd_writer.cpp
    test_ovrtx_usd.cpp
)

target_include_directories(test_ovrtx_usd PRIVATE ${MayoOvrtx_Root}/src/ovrtx)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_ovrtx_usd PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_test(NAME test_ovrtx_usd COMMAND test_ovrtx_usd)
