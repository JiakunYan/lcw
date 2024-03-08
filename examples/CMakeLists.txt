function(add_lcw_examples)
  foreach(name ${ARGN})
    string(REGEX REPLACE "\\.[^.]*$" "" name_without_ext ${name})
    add_lcw_executable(${name_without_ext} ${name} comm_exp.hpp)
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
  REQUIRED
  HINTS
  ${LCI_ROOT}
  $ENV{LCI_ROOT}
  PATH_SUFFIXES
  lib/cmake
  lib64/cmake)
target_link_libraries(pingpong_mt PRIVATE LCI::LCT)

add_subdirectory(mpi)
