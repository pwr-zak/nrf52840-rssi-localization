# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky)

target_sources(app PRIVATE src/main.c)

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/app-version.h
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/generate-app-version-h
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(run_prebuild_script DEPENDS ${CMAKE_BINARY_DIR}/app-version.h)
add_dependencies(app run_prebuild_script)

target_include_directories(app PRIVATE ${CMAKE_BINARY_DIR})

