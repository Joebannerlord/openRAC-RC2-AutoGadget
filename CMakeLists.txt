# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)
project(rac2_auto_gadgets LANGUAGES CXX)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" sdk_lock)
string(JSON pinned_sdk GET "${sdk_lock}" version)
set(OPENRAC_SDK "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${pinned_sdk}"
    CACHE PATH "Unpacked OpenRAC Mod SDK")
if(NOT EXISTS "${OPENRAC_SDK}/VERSION")
  message(FATAL_ERROR "Run python tools/fetch_sdk.py to fetch OpenRAC Mod SDK ${pinned_sdk}")
endif()
file(STRINGS "${OPENRAC_SDK}/VERSION" actual_sdk LIMIT_COUNT 1)
if(NOT actual_sdk STREQUAL pinned_sdk)
  message(FATAL_ERROR "Expected OpenRAC Mod SDK ${pinned_sdk}, found ${actual_sdk}")
endif()
set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
find_package(OpenRACMod 1.20 EXACT CONFIG REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

if(NOT OPENRAC_MOD_PLATFORM STREQUAL "windows-x86_64")
  message(FATAL_ERROR "Auto Gadgets supports Windows x86-64.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(mod_target rac2_auto_gadgets_014)
openrac_add_mod(${mod_target} MANIFEST openrac-mod.json SOURCES src/mod.cpp src/common/json.cpp)
target_include_directories(${mod_target} PRIVATE src)
get_target_property(package_dir ${mod_target} OPENRAC_MOD_PACKAGE_DIR)

add_custom_target(release_zip
  COMMAND "${Python3_EXECUTABLE}" "${OPENRAC_SDK}/tools/package_mod.py"
          "${package_dir}" -o "${CMAKE_CURRENT_BINARY_DIR}/dist"
  DEPENDS ${mod_target}
  VERBATIM)

include(CTest)
set(WRENCH_TEST_DLL "" CACHE FILEPATH "Contextual Wrench and Fire 0.1.1 DLL used by the offline test")
if(BUILD_TESTING AND WRENCH_TEST_DLL)
  add_executable(plugin_test tests/plugin_test.cpp)
  target_include_directories(plugin_test PRIVATE "${OPENRAC_SDK}/include" src)
  foreach(delay IN ITEMS 0 2 5)
    add_test(NAME auto_gadgets_delay_${delay} COMMAND plugin_test
      "${package_dir}/lib/windows-x86_64/$<TARGET_FILE_NAME:${mod_target}>"
      "${WRENCH_TEST_DLL}" ${delay})
  endforeach()
endif()
