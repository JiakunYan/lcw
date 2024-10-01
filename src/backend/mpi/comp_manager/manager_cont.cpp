#include "lcwi.hpp"

#ifdef LCW_MPI_USE_CONT

namespace lcw
{
namespace mpi
{
namespace comp
{
static int complete_cb(int status, void* user_data)
{
  LCW_Assert(user_data != nullptr, "");
  LCW_Assert(status == MPI_SUCCESS, "");
  auto* entry_p = static_cast<entry_t*>(user_data);
  LCW_Log(LCW_LOG_TRACE, "comp", "Invoke continuation %p\n", entry_p->mpi_req);
  push_cq(*entry_p);
  delete entry_p;
  return MPI_SUCCESS;
}

void manager_cont_t::add_entry(entry_t entry)
{
  auto entry_p = new entry_t(entry);
  LCW_Log(LCW_LOG_TRACE, "comp", "Attach continuation %p\n", entry.mpi_req);
  mpi::enter_stream_cs(entry.device);
  MPI_SAFECALL(MPIX_Continue(&entry.mpi_req, &complete_cb, entry_p,
                             mpi::config.cont_flag, MPI_STATUS_IGNORE,
                             cont_req));
  mpi::leave_stream_cs(entry.device);
}

bool manager_cont_t::do_progress() { return false; }
}  // namespace comp
}  // namespace mpi
}  // namespace lcw

#endif  // LCW_MPI_USE_CONT