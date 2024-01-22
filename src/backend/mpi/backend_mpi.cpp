#include <vector>
#include <cstring>
#include <mpi.h>
#include "lcwi.hpp"

namespace lcw
{
int g_rank = -1;
int g_nranks = -1;

void backend_mpi_t::initialize()
{
  int provided;
  MPI_SAFECALL(
      MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided));
  LCW_Assert(provided == MPI_THREAD_MULTIPLE,
             "Cannot enable multithreaded MPI!\n");
  MPI_SAFECALL(MPI_Comm_rank(MPI_COMM_WORLD, &g_rank));
  MPI_SAFECALL(MPI_Comm_size(MPI_COMM_WORLD, &g_nranks));
}

void backend_mpi_t::finalize() { MPI_SAFECALL(MPI_Finalize()); }

int64_t backend_mpi_t::get_rank() { return g_rank; }

int64_t backend_mpi_t::get_nranks() { return g_nranks; }

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

struct device_t {
  MPI_Comm comm_2sided;
  MPI_Comm comm_1sided;
  std::vector<char> put_rbuf;
  tag_t max_tag;
};

void push_cq(comp_t completion, mpi::cq_entry_t entry)
{
  auto* cq = reinterpret_cast<mpi::cq_t*>(completion);
  cq->lock.lock();
  cq->entries.push_back(entry);
  cq->lock.unlock();
}

const int PUT_SIGNAL_TAG = 0;
}  // namespace mpi

void post_put_recv(device_t device, comp_t completion)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::cq_entry_t entry = {
      .request = MPI_REQUEST_NULL,
      .context = {
          .op = op_t::PUT_SIGNAL,
          .device = device,
          .rank = -1,
          .tag = mpi::PUT_SIGNAL_TAG,
          .buffer = device_p->put_rbuf.data(),
          .length = static_cast<int64_t>(device_p->put_rbuf.size()),
          .user_context = nullptr,
      }};
  MPI_SAFECALL(MPI_Irecv(entry.context.buffer, entry.context.length, MPI_CHAR,
                         MPI_ANY_SOURCE, entry.context.tag,
                         device_p->comm_1sided, &entry.request));
  mpi::push_cq(completion, entry);
}

device_t backend_mpi_t::alloc_device(int64_t max_put_length, comp_t put_comp)
{
  auto* device_p = new mpi::device_t;
  auto device = reinterpret_cast<device_t>(device_p);
  MPI_SAFECALL(MPI_Comm_dup(MPI_COMM_WORLD, &device_p->comm_2sided));
  // get max tag
  device_p->max_tag = 32767;
  void* max_tag_p;
  int flag;
  MPI_Comm_get_attr(device_p->comm_2sided, MPI_TAG_UB, &max_tag_p, &flag);
  if (flag) device_p->max_tag = *(int*)max_tag_p;
  // 1sided
  if (max_put_length > 0) {
    device_p->put_rbuf.resize(max_put_length);
    MPI_SAFECALL(MPI_Comm_dup(MPI_COMM_WORLD, &device_p->comm_1sided));
    post_put_recv(device, put_comp);
  }
  return device;
}

void backend_mpi_t::free_device(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  MPI_Comm_free(&device_p->comm_2sided);
  if (!device_p->put_rbuf.empty()) {
    MPI_Comm_free(&device_p->comm_1sided);
  }
}

bool backend_mpi_t::do_progress(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  return false;
}

comp_t backend_mpi_t::alloc_cq()
{
  auto* cq = new mpi::cq_t;
  return reinterpret_cast<comp_t>(cq);
}

void backend_mpi_t::free_cq(comp_t completion)
{
  auto* cq = reinterpret_cast<mpi::cq_t*>(completion);
  while (!cq->entries.empty()) {
    auto entry = cq->entries.front();
    cq->entries.pop_front();
  }
  delete cq;
}

bool backend_mpi_t::poll_cq(comp_t completion, request_t* request)
{
  auto* cq = reinterpret_cast<mpi::cq_t*>(completion);
  if (cq->entries.empty() || !cq->lock.try_lock()) return false;
  if (cq->entries.empty()) {
    cq->lock.unlock();
    return false;
  }
  auto entry = cq->entries.front();
  cq->entries.pop_front();
  int succeed = 0;
  MPI_Status status;
  if (entry.request == MPI_REQUEST_NULL)
    succeed = 1;
  else {
    MPI_SAFECALL(MPI_Test(&entry.request, &succeed, &status));
  }
  if (succeed) {
    *request = entry.context;
  } else {
    cq->entries.push_back(entry);
  }
  cq->lock.unlock();
  if (!succeed) return false;
  if (request->op == op_t::RECV || request->op == op_t::PUT_SIGNAL) {
    int count;
    MPI_SAFECALL(MPI_Get_count(&status, MPI_CHAR, &count));
    request->length = count;
    request->tag = status.MPI_TAG;
    request->rank = status.MPI_SOURCE;
  }
  if (request->op == op_t::PUT_SIGNAL) {
    // Copy the data out and repost the receive
    void* buffer;
    int ret = posix_memalign(&buffer, LCW_CACHE_LINE, request->length);
    LCW_Assert(ret == 0, "posix_memalign(%ld) failed!\n", request->length);
    memcpy(buffer, request->buffer, request->length);
    request->buffer = buffer;
    post_put_recv(request->device, completion);
  }
  return succeed;
}

bool backend_mpi_t::send(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
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
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
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
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::cq_entry_t entry = {.request = MPI_REQUEST_NULL,
                           .context = {
                               .op = op_t::PUT,
                               .device = device,
                               .rank = rank,
                               .tag = mpi::PUT_SIGNAL_TAG,
                               .buffer = buf,
                               .length = length,
                               .user_context = user_context,
                           }};
  MPI_SAFECALL(MPI_Isend(entry.context.buffer, entry.context.length, MPI_CHAR,
                         entry.context.rank, entry.context.tag,
                         device_p->comm_1sided, &entry.request));
  push_cq(completion, entry);
  return true;
}

tag_t backend_mpi_t::get_max_tag(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  return device_p->max_tag;
}

}  // namespace lcw