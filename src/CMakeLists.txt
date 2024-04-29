add_library(lcw)
target_sources(lcw PRIVATE lcw.cpp env.cpp logger/logger.cpp
                           backend/backend.cpp)
target_include_directories(
  lcw
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

option(LCW_TRY_ENABLE_BACKEND_LCI "whether to enable the LCI backend" ON)

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
target_link_libraries(lcw PRIVATE LCI::LCT)
if(LCW_TRY_ENABLE_BACKEND_LCI AND NOT LCI_WITH_LCT_ONLY)
  set(LCW_ENABLE_BACKEND_LCI ON)
  target_link_libraries(lcw PRIVATE LCI::LCI)
  target_sources(lcw PRIVATE backend/lci/backend_lci.cpp)
else()
  message(WARNING "LCI is not found. The LCI backend if not enabled.")
endif()

option(LCW_TRY_ENABLE_BACKEND_MPI "whether to enable the MPI backend" ON)

if(LCW_TRY_ENABLE_BACKEND_MPI)
  find_package(MPI)
  if(MPI_FOUND)
    set(LCW_ENABLE_BACKEND_MPI ON)
    target_link_libraries(lcw PRIVATE MPI::MPI_CXX)
    target_sources(
      lcw
      PRIVATE backend/mpi/backend_mpi.cpp
              backend/mpi/comp_manager/manager_req.cpp
              backend/mpi/comp_manager/manager_cont.cpp)
  else()
    message(WARNING "MPI is not found. The MPI backend if not enabled.")
  endif()
endif()

add_subdirectory(api)
