add_library(lcw)

if(NOT LCW_ROOT_PROJECT)
  add_library(LCW::lcw ALIAS lcw)
endif()

set_target_properties(
  lcw
  PROPERTIES CXX_STANDARD 17
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)
target_sources(
  lcw
  PRIVATE lcw.cpp
          env.cpp
          logger/logger.cpp
          backend/backend.cpp
          util/spinlock.cpp
          util/pcounter.cpp
          comp/cq/cq.cpp
          comp/handler/handler.cpp)
target_include_directories(
  lcw
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# LCI
option(LCW_TRY_ENABLE_BACKEND_LCI "whether to enable the LCI backend" ON)

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
message(STATUS "LCI_WITH_LCT_ONLY: ${LCI_WITH_LCT_ONLY}")
message(STATUS "LCI_VERSION: ${LCI_VERSION}")

target_link_libraries(lcw PRIVATE LCI::LCT)
if(LCW_TRY_ENABLE_BACKEND_LCI
   AND NOT LCI_WITH_LCT_ONLY
   AND LCI_VERSION VERSION_LESS 2.0.0)
  set(LCW_ENABLE_BACKEND_LCI ON)
  target_link_libraries(lcw PRIVATE LCI::LCI)
  target_sources(lcw PRIVATE backend/lci/backend_lci.cpp)
else()
  message(WARNING "LCI is not found. The LCI backend if not enabled.")
endif()

# LCI2
option(LCW_TRY_ENABLE_BACKEND_LCI2 "whether to enable the LCI2 backend" ON)
if(LCW_TRY_ENABLE_BACKEND_LCI2
   AND NOT LCI_WITH_LCT_ONLY
   AND LCI_VERSION VERSION_GREATER_EQUAL 2.0.0)
  set(LCW_ENABLE_BACKEND_LCI2 ON)
  target_link_libraries(lcw PRIVATE LCI::lci)
  target_sources(lcw PRIVATE backend/lci2/backend_lci2.cpp)
else()
  message(WARNING "LCI2 is not found. The LCI2 backend if not enabled.")
endif()

# MPI
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

# GASNet-EX
option(LCW_TRY_ENABLE_BACKEND_GEX "whether to enable the MPI backend" ON)

if(LCW_TRY_ENABLE_BACKEND_GEX)
  set(LCW_USE_GASNET_MODULE
      gasnet-ibv-par
      CACHE STRING "Gasnet module to use")
  option(LCW_USE_GASNET_NEED_MPI "Gasnet module needs MPI" ON)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(gasnet IMPORTED_TARGET ${LCW_USE_GASNET_MODULE})
  if(gasnet_FOUND)
    if(LCW_USE_GASNET_NEED_MPI)
      find_package(MPI REQUIRED)
      target_include_directories(lcw PRIVATE MPI::MPI_CXX)
    endif()

    # fix a cmake bug when encountering multiple "--param <val>" options
    # Reference:
    # https://cmake.org/cmake/help/latest/command/target_compile_options.html#option-de-duplication
    macro(fix_property TARGET PROPERTY)
      get_target_property(PROPERTY_OLD ${TARGET} ${PROPERTY})
      string(REPLACE "--param;" "SHELL:--param " PROPERTY_NEW "${PROPERTY_OLD}")
      set_target_properties(${TARGET} PROPERTIES ${PROPERTY} "${PROPERTY_NEW}")
      message(
        STATUS "Fix ${TARGET} ${PROPERTY}: ${PROPERTY_OLD} -> ${PROPERTY_NEW}")
    endmacro()
    fix_property(PkgConfig::gasnet INTERFACE_COMPILE_OPTIONS)
    fix_property(PkgConfig::gasnet INTERFACE_LINK_OPTIONS)

    set(LCW_ENABLE_BACKEND_GEX ON)
    target_link_libraries(lcw PRIVATE PkgConfig::gasnet)
    target_sources(lcw PRIVATE backend/gex/backend_gex.cpp)
  else()
    message(WARNING "GASNet-EX is not found. The GEX backend if not enabled.")
  endif()
endif()

add_subdirectory(api)
