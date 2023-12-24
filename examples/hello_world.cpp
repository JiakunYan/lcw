#include "lcw.hpp"
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

int main(int argc, char** args)
{
  char hostname[HOST_NAME_MAX + 1];
  gethostname(hostname, HOST_NAME_MAX + 1);
  lcw::initialize(lcw::backend_t::MPI);
  printf("%s: %ld / %ld OK\n", hostname, lcw::get_rank(), lcw::get_nranks());
  lcw::finalize();
}
