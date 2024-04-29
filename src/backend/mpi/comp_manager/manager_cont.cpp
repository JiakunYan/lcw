#include "manager_cont.hpp"
#include "backend/mpi/backend_mpi.hpp"

#ifdef LCW_COMP_MANAGER_CONT_ENABLED

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
  auto cq = reinterpret_cast<LCT_queue_t>(entry_p->completion);
  LCT_queue_push(cq, entry_p->request);
  delete entry_p;
  return MPI_SUCCESS;
}

void manager_cont_t::add_entry(entry_t entry)
{
  auto entry_p = new entry_t(entry);
  LCW_Log(LCW_LOG_TRACE, "comp", "Attach continuation %p\n", entry.mpi_req);
  MPI_SAFECALL(MPIX_Continue(&entry.mpi_req, &complete_cb, entry_p,
                             MPIX_CONT_IMMEDIATE | MPIX_CONT_FORGET,
                             MPI_STATUS_IGNORE, cont_req));
}

bool manager_cont_t::do_progress()
{
#ifdef MPIX_STREAM_NULL
  MPI_SAFECALL(MPIX_Stream_progress(MPIX_STREAM_NULL));
#endif
  return false;
}
}  // namespace comp
}  // namespace mpi
}  // namespace lcw

#endif  // LCW_COMP_MANAGER_CONT_ENABLED