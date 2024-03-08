find_package(MPI)
if(MPI_FOUND)
  add_executable(mpi_hello_world mpi_hello_world.cpp)
  target_link_libraries(mpi_hello_world PRIVATE MPI::MPI_CXX)
  set_target_properties(mpi_hello_world PROPERTIES OUTPUT_NAME
                                                   "lcw_mpi_hello_world")
  install(TARGETS mpi_hello_world RUNTIME)
endif()
