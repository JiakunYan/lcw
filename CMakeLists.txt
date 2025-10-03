cmake_minimum_required(VERSION 3.12)
project(
  LCW
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "Lightweight Communication Wrapper"
  HOMEPAGE_URL "https://github.com/JiakunYan/lcw")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(LCW_ROOT_PROJECT ON)
else()
  set(LCW_ROOT_PROJECT OFF)
endif()

if(LCW_ROOT_PROJECT)
  enable_testing()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(AddLCW)

# ##############################################################################
# General Options
# ##############################################################################
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

# ##############################################################################
# What parts of LCW to build
# ##############################################################################
option(LCW_WITH_EXAMPLES "Build LCW examples" ON)

# ##############################################################################
# LCW Options
# ##############################################################################
option(LCW_DEBUG "Enable debugging build" OFF)
set(LCW_CACHE_LINE
    64
    CACHE STRING "Size of cache line (bytes)")
option(LCW_ENABLE_PCOUNTER "Enable performance counters" OFF)

# ##############################################################################
# LCI Testing related options
# ##############################################################################
set(LCW_USE_CTEST_LAUNCHER
    lcrun
    CACHE STRING "exective to be used in ctest")
set(LCW_USE_CTEST_ARGS
    ""
    CACHE STRING "arguments to be used in ctest")

# ##############################################################################
# Add subdirectories
# ##############################################################################
add_subdirectory(src)
if(LCW_ROOT_PROJECT)
  if(LCW_WITH_EXAMPLES)
    add_subdirectory(examples)
  endif()
endif()

# ##############################################################################
# Install
# ##############################################################################
if(LCW_ROOT_PROJECT)
  configure_file(liblcw.pc.in liblcw.pc @ONLY)

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/LCWConfigVersion.cmake"
    COMPATIBILITY ExactVersion)
  configure_package_config_file(
    LCWConfig.cmake.in LCWConfig.cmake
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake"
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR)
  install(
    TARGETS lcw
    EXPORT LCWTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(
    DIRECTORY src/api/ ${CMAKE_CURRENT_BINARY_DIR}/src/api/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING
    PATTERN "*.hpp")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/liblcw.pc"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
  install(
    EXPORT LCWTargets
    FILE LCWTargets.cmake
    NAMESPACE LCW::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LCW)
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/LCWConfig.cmake"
                "${CMAKE_CURRENT_BINARY_DIR}/LCWConfigVersion.cmake"
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
endif()
