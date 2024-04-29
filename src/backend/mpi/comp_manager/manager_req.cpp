#include "manager_req.hpp"

namespace lcw
{
namespace mpi
{
namespace comp
{
void manager_req_t::add_entry(entry_t entry)
{
  LCW_Log(LCW_LOG_TRACE, "comp", "Add request %p\n", entry.mpi_req);
  lock.lock();
  entries.push_back(entry);
  lock.unlock();
}

bool manager_req_t::do_progress()
{
  if (entries.empty() || !lock.try_lock()) return false;
  if (entries.empty()) {
    lock.unlock();
    return false;
  }
  MPI_Status status;
  int succeed = 0;
  entry_t entry = entries.front();
  entries.pop_front();
  auto req = entry.mpi_req;
  if (entry.mpi_req == MPI_REQUEST_NULL)
    succeed = 1;
  else {
    MPI_SAFECALL(MPI_Test(&entry.mpi_req, &succeed, &status));
  }
  if (!succeed) {
    entries.push_back(entry);
  }
  lock.unlock();
  if (!succeed) return false;
  // We have got something
  LCW_Assert(entry.request->op != op_t::PUT_SIGNAL, "Unexpected op\n");
  if (entry.request->op == op_t::RECV) {
    int count;
    MPI_SAFECALL(MPI_Get_count(&status, MPI_CHAR, &count));
    entry.request->length = count;
    entry.request->tag = status.MPI_TAG;
    entry.request->rank = status.MPI_SOURCE;
  }
  auto cq = reinterpret_cast<LCT_queue_t>(entry.completion);
  LCW_Log(LCW_LOG_TRACE, "comp", "Release entry %p\n", req);
  LCT_queue_push(cq, entry.request);
  return true;
}
}  // namespace comp
}  // namespace mpi
}  // namespace lcw
