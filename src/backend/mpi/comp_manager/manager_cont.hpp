#ifndef LCW_MANAGER_CONT_HPP
#define LCW_MANAGER_CONT_HPP

#include "lcwi.hpp"

#ifdef LCW_MPI_USE_CONT
#include "manager_base.hpp"

namespace lcw
{
namespace mpi
{
namespace comp
{
struct manager_cont_t : public manager_base_t {
  manager_cont_t() : cont_req(MPI_REQUEST_NULL)
  {
    if (mpi::config.use_cont_req) {
      MPI_SAFECALL(MPIX_Continue_init(0, 0, MPI_INFO_NULL, &cont_req));
      MPI_SAFECALL(MPI_Start(&cont_req));
    }
  }
  ~manager_cont_t()
  {
    if (cont_req != MPI_REQUEST_NULL) {
      MPI_SAFECALL(MPI_Request_free(&cont_req));
    }
  }
  void add_entry(entry_t entry) override;
  bool do_progress() override;
  MPI_Request cont_req;
  std::deque<entry_t> entries;
  spinlock_t lock;
};
}  // namespace comp
}  // namespace mpi
}  // namespace lcw
#endif  // LCW_MPI_USE_CONT

#endif  // LCW_MANAGER_CONT_HPP
