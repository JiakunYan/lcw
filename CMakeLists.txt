cmake_minimum_required(VERSION 3.12)
project(
  LCW
  VERSION 0.1.0
  LANGUAGES CXX
  DESCRIPTION "Lightweight Communication Wrapper"
  HOMEPAGE_URL "https://github.com/JiakunYan/lcw")
enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

include(AddLCW)

# ##############################################################################
# General Options
# ##############################################################################
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

add_subdirectory(src)
add_subdirectory(examples)
