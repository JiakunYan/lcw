#include "lcwi.hpp"

namespace lcw
{
int rank = -1;
int nranks = -1;

void backend_mpi_t::initialize()
{
  int provided;
  MPI_SAFECALL(
      MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided));
  LCW_Assert(provided == MPI_THREAD_MULTIPLE,
             "Cannot enable multithreaded MPI!\n");
  MPI_SAFECALL(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
  MPI_SAFECALL(MPI_Comm_size(MPI_COMM_WORLD, &nranks));
}

void backend_mpi_t::finalize() { MPI_SAFECALL(MPI_Finalize()); }

int64_t backend_mpi_t::get_rank() { return rank; }

int64_t backend_mpi_t::get_nranks() { return nranks; }

struct device_mpi_t {
  MPI_Comm comm_2sided;
  MPI_Comm comm_1sided;
};

device_t backend_mpi_t::alloc_device()
{
  auto* device_p = new device_mpi_t;
  MPI_Comm_dup(MPI_COMM_WORLD, &device_p->comm_2sided);
  MPI_Comm_dup(MPI_COMM_WORLD, &device_p->comm_1sided);
  return reinterpret_cast<device_t>(device_p);
}

void backend_mpi_t::free_device(device_t* device)
{
  auto* device_p = reinterpret_cast<device_mpi_t*>(device);
  MPI_Comm_free(&device_p->comm_2sided);
  MPI_Comm_free(&device_p->comm_1sided);
}

bool backend_mpi_t::do_progress(device_t device)
{
  auto* device_p = reinterpret_cast<device_mpi_t*>(device);
  return false;
}

namespace mpi
{
struct cq_entry_t {
  MPI_Request request;
  request_t context;
};

struct cq_t {
  std::deque<cq_entry_t> entries;
  spinlock_t lock;
};
}  // namespace mpi

comp_t backend_mpi_t::alloc_cq()
{
  auto* cq = new mpi::cq_t;
  return reinterpret_cast<comp_t>(cq);
}

void backend_mpi_t::free_cq(comp_t completion)
{
  auto* cq = reinterpret_cast<mpi::cq_t*>(completion);
  delete cq;
}

bool backend_mpi_t::poll_cq(comp_t completion, request_t* request)
{
  auto* cq = reinterpret_cast<mpi::cq_t*>(completion);
  if (!cq->lock.try_lock()) return false;
  auto entry = cq->entries.front();
  cq->entries.pop_front();
  int flag = 0;
  MPI_SAFECALL(MPI_Test(&entry.request, &flag, MPI_STATUS_IGNORE));
  if (flag) {
    *request = entry.context;
  } else {
    cq->entries.push_back(entry);
  }
  cq->lock.unlock();
  return flag;
}

void push_cq(comp_t completion, mpi::cq_entry_t entry)
{
  auto* cq = reinterpret_cast<mpi::cq_t*>(completion);
  cq->lock.lock();
  cq->entries.push_back(entry);
  cq->lock.unlock();
}

bool backend_mpi_t::send(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<device_mpi_t*>(device);
  mpi::cq_entry_t entry = {.request = MPI_REQUEST_NULL,
                           .context = {
                               .op = op_t::SEND,
                               .device = device,
                               .rank = rank,
                               .tag = tag,
                               .buffer = buf,
                               .length = length,
                               .user_context = user_context,
                           }};
  MPI_SAFECALL(MPI_Isend(buf, length, MPI_CHAR, rank, tag,
                         device_p->comm_2sided, &entry.request));
  push_cq(completion, entry);
  return true;
}

bool backend_mpi_t::recv(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<device_mpi_t*>(device);
  mpi::cq_entry_t entry = {.request = MPI_REQUEST_NULL,
                           .context = {
                               .op = op_t::RECV,
                               .device = device,
                               .rank = rank,
                               .tag = tag,
                               .buffer = buf,
                               .length = length,
                               .user_context = user_context,
                           }};
  MPI_SAFECALL(MPI_Irecv(buf, length, MPI_CHAR, rank, tag,
                         device_p->comm_2sided, &entry.request));
  push_cq(completion, entry);
  return true;
}

bool backend_mpi_t::put(device_t device, rank_t rank, void* buf, int64_t length,
                        comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<device_mpi_t*>(device);
  mpi::cq_entry_t entry = {.request = MPI_REQUEST_NULL,
                           .context = {
                               .op = op_t::SEND,
                               .device = device,
                               .rank = rank,
                               .tag = -1,
                               .buffer = buf,
                               .length = length,
                               .user_context = user_context,
                           }};
  MPI_SAFECALL(MPI_Isend(buf, length, MPI_CHAR, rank, -1, device_p->comm_2sided,
                         &entry.request));
  push_cq(completion, entry);
  return true;
}

}  // namespace lcw