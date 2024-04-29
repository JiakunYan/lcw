#ifndef LCW_MANAGER_BASE_HPP
#define LCW_MANAGER_BASE_HPP

#include <mpi.h>
#include "lcwi.hpp"

namespace lcw
{
namespace mpi
{
namespace comp
{
struct entry_t {
  MPI_Request mpi_req;
  device_t device;
  comp_t completion;
  request_t* request;
};

struct manager_base_t {
  manager_base_t() = default;
  virtual ~manager_base_t() = default;
  virtual void add_entry(entry_t entry) = 0;
  virtual bool do_progress() = 0;
};
}  // namespace comp
}  // namespace mpi
}  // namespace lcw

#endif  // LCW_MANAGER_BASE_HPP
