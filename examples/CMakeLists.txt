function(add_lcw_examples)
  foreach(name ${ARGN})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lcw_executable(${name_without_ext} ${name} comm_exp.hpp)
    add_lcw_test(
      ${name_without_ext} LABELS example COMMANDS
      "${LCW_USE_CTEST_LAUNCHER} -n 2 ${LCW_USE_CTEST_ARGS} [TARGET]")
  endforeach()
endfunction()

option(LCI_BUILD_EXAMPLES "Build examples by default" ON)
if(NOT LCI_BUILD_EXAMPLES)
  set(EXCLUDE_FROM_ALL ON)
endif()

add_lcw_examples(hello_world.cpp pingpong_mt.cpp)

find_package(Threads REQUIRED)
target_link_libraries(pingpong_mt PRIVATE Threads::Threads)

find_package(
  LCI
  CONFIG
  HINTS
  ${LCI_ROOT}
  $ENV{LCI_ROOT}
  PATH_SUFFIXES
  lib/cmake
  lib64/cmake)
if(NOT LCI_FOUND)
  message(STATUS "Existing LCI installation not found. Try FetchContent.")
  include(FetchContent)
  FetchContent_Declare(
    lci
    GIT_REPOSITORY https://github.com/uiuc-hpc/lci.git
    GIT_TAG master)
  FetchContent_MakeAvailable(lci)
endif()
target_link_libraries(pingpong_mt PRIVATE LCI::LCT)

add_lcw_test(
  pingpong_mt
  LABELS
  pingpong
  COMMANDS
  "${LCW_USE_CTEST_LAUNCHER} -n 2 ${LCW_USE_CTEST_ARGS} $<TARGET_FILE:pingpong_mt> --op send --nprgthreads 0"
  "${LCW_USE_CTEST_LAUNCHER} -n 2 ${LCW_USE_CTEST_ARGS} $<TARGET_FILE:pingpong_mt> --op put --nprgthreads 0"
  "${LCW_USE_CTEST_LAUNCHER} -n 2 ${LCW_USE_CTEST_ARGS} $<TARGET_FILE:pingpong_mt> --op send --send-window 4 --recv-window 1 --nprgthreads 0"
  "${LCW_USE_CTEST_LAUNCHER} -n 2 ${LCW_USE_CTEST_ARGS} $<TARGET_FILE:pingpong_mt> --op put --send-window 4 --recv-window 1 --nprgthreads 0 --ndevices 4 --nthreads 4"
  "${LCW_USE_CTEST_LAUNCHER} -n 2 ${LCW_USE_CTEST_ARGS} $<TARGET_FILE:pingpong_mt> --op send --send-window 4 --recv-window 4 --nprgthreads 0"
  "${LCW_USE_CTEST_LAUNCHER} -n 2 ${LCW_USE_CTEST_ARGS} $<TARGET_FILE:pingpong_mt> --op put --send-window 4 --recv-window 4 --nprgthreads 0 --ndevices 4 --nthreads 4"
)

add_subdirectory(mpi)
