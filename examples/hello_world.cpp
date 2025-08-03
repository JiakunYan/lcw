#include "lcw.hpp"
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

int main(int argc, char** args)
{
  char hostname[HOST_NAME_MAX + 1];
  gethostname(hostname, HOST_NAME_MAX + 1);
  lcw::initialize();
  printf("%s: %ld / %ld OK\n", hostname, lcw::get_rank(), lcw::get_nranks());
  lcw::finalize();
}
