#include <vector>
#include <cstring>
#include <mpi.h>
#include "lcwi.hpp"

namespace lcw
{
int g_rank = -1;
int g_nranks = -1;
LCT_queue_type_t cq_type;
int LCW_MPI_DEFAULT_QUEUE_LENGTH = 65536;

void backend_mpi_t::initialize()
{
  int provided;
  MPI_SAFECALL(
      MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided));
  LCW_Assert(provided == MPI_THREAD_MULTIPLE,
             "Cannot enable multithreaded MPI!\n");
  MPI_SAFECALL(MPI_Comm_rank(MPI_COMM_WORLD, &g_rank));
  MPI_SAFECALL(MPI_Comm_size(MPI_COMM_WORLD, &g_nranks));

  // Completion queue type
  const LCT_queue_type_t cq_type_default = LCT_QUEUE_ARRAY_ATOMIC_FAA;
  LCT_dict_str_int_t dict[] = {
      {NULL, cq_type_default},
      {"array_atomic_faa", LCT_QUEUE_ARRAY_ATOMIC_FAA},
      {"array_atomic_cas", LCT_QUEUE_ARRAY_ATOMIC_CAS},
      {"array_atomic_basic", LCT_QUEUE_ARRAY_ATOMIC_BASIC},
      {"array_mutex", LCT_QUEUE_ARRAY_MUTEX},
      {"std_mutex", LCT_QUEUE_STD_MUTEX},
  };
  bool succeed = LCT_str_int_search(dict, sizeof(dict) / sizeof(dict[0]),
                                    getenv("LCW_MPI_CQ_TYPE"), cq_type_default,
                                    (int*)&cq_type);
  if (!succeed) {
    LCW_Warn("Unknown LCI_CQ_TYPE %s. Use the default type: array_atomic_faa\n",
             getenv("LCI_CQ_TYPE"));
  }
  LCW_Log(LCW_LOG_INFO, "comp", "Set LCW_MPI_CQ_TYPE to %d\n", cq_type);

  // Completion queue length
  {
    char* p = getenv("LCW_MPI_DEFAULT_QUEUE_LENGTH");
    if (p) LCW_MPI_DEFAULT_QUEUE_LENGTH = atoi(p);
  }
}

void backend_mpi_t::finalize() { MPI_SAFECALL(MPI_Finalize()); }

int64_t backend_mpi_t::get_rank() { return g_rank; }

int64_t backend_mpi_t::get_nranks() { return g_nranks; }

namespace mpi
{
struct progress_entry_t {
  MPI_Request mpi_req;
  comp_t completion;
  request_t* request;
};

struct progress_engine_t {
  progress_entry_t put_entry;
  spinlock_t put_entry_lock;
  std::deque<progress_entry_t> entries;
  spinlock_t entries_lock;
};

struct device_t {
  MPI_Comm comm_2sided;
  MPI_Comm comm_1sided;
  std::vector<char> put_rbuf;
  tag_t max_tag;
  progress_engine_t pengine;
};

void add_to_progress_engine(mpi::device_t* device, mpi::progress_entry_t entry)
{
  device->pengine.entries_lock.lock();
  device->pengine.entries.push_back(entry);
  device->pengine.entries_lock.unlock();
}

const int PUT_SIGNAL_TAG = 0;
}  // namespace mpi

void post_put_recv(device_t device, comp_t completion)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::progress_entry_t entry = {
      .mpi_req = MPI_REQUEST_NULL,
      .completion = completion,
      .request = new request_t{
          .op = op_t::PUT_SIGNAL,
          .device = device,
          .rank = -1,
          .tag = mpi::PUT_SIGNAL_TAG,
          .buffer = device_p->put_rbuf.data(),
          .length = static_cast<int64_t>(device_p->put_rbuf.size()),
          .user_context = nullptr,
      }};
  MPI_SAFECALL(MPI_Irecv(entry.request->buffer, entry.request->length, MPI_CHAR,
                         MPI_ANY_SOURCE, entry.request->tag,
                         device_p->comm_1sided, &entry.mpi_req));
  device_p->pengine.put_entry = entry;
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
  } else {
    device_p->pengine.put_entry.mpi_req = MPI_REQUEST_NULL;
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
  delete device_p;
}

bool backend_mpi_t::do_progress(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  // work on put
  int succeed = 0;
  MPI_Status status;
  mpi::progress_entry_t entry;
  if (device_p->pengine.put_entry.mpi_req != MPI_REQUEST_NULL &&
      device_p->pengine.put_entry_lock.try_lock()) {
    if (device_p->pengine.put_entry.mpi_req != MPI_REQUEST_NULL) {
      MPI_SAFECALL(
          MPI_Test(&device_p->pengine.put_entry.mpi_req, &succeed, &status));
      if (succeed) {
        entry = device_p->pengine.put_entry;
        int count;
        MPI_SAFECALL(MPI_Get_count(&status, MPI_CHAR, &count));
        entry.request->length = count;
        entry.request->tag = status.MPI_TAG;
        entry.request->rank = status.MPI_SOURCE;
        // Copy the data out and repost the receive
        LCW_Assert(entry.request->op == op_t::PUT_SIGNAL, "Unexpected op\n");
        void* buffer;
        int ret =
            posix_memalign(&buffer, LCW_CACHE_LINE, entry.request->length);
        LCW_Assert(ret == 0, "posix_memalign(%ld) failed!\n",
                   entry.request->length);
        memcpy(buffer, entry.request->buffer, entry.request->length);
        entry.request->buffer = buffer;
        post_put_recv(entry.request->device, entry.completion);
      }
    }
    device_p->pengine.put_entry_lock.unlock();
  }
  if (!succeed) {
    // work on sendrecv
    if (device_p->pengine.entries.empty() ||
        !device_p->pengine.entries_lock.try_lock())
      return false;
    if (device_p->pengine.entries.empty()) {
      device_p->pengine.entries_lock.unlock();
      return false;
    }
    entry = device_p->pengine.entries.front();
    device_p->pengine.entries.pop_front();
    if (entry.mpi_req == MPI_REQUEST_NULL)
      succeed = 1;
    else {
      MPI_SAFECALL(MPI_Test(&entry.mpi_req, &succeed, &status));
    }
    if (!succeed) {
      device_p->pengine.entries.push_back(entry);
    }
    device_p->pengine.entries_lock.unlock();
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
  }
  auto cq = reinterpret_cast<LCT_queue_t>(entry.completion);
  LCT_queue_push(cq, entry.request);
  return true;
}

comp_t backend_mpi_t::alloc_cq()
{
  LCT_queue_t cq = LCT_queue_alloc(cq_type, LCW_MPI_DEFAULT_QUEUE_LENGTH);
  return reinterpret_cast<comp_t>(cq);
}

void backend_mpi_t::free_cq(comp_t completion)
{
  auto cq = reinterpret_cast<LCT_queue_t>(completion);
  LCT_queue_free(&cq);
}

bool backend_mpi_t::poll_cq(comp_t completion, request_t* request)
{
  auto cq = reinterpret_cast<LCT_queue_t>(completion);
  auto* req = static_cast<request_t*>(LCT_queue_pop(cq));
  if (req == nullptr) return false;
  *request = *req;
  return true;
}

bool backend_mpi_t::send(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::progress_entry_t entry = {.mpi_req = MPI_REQUEST_NULL,
                                 .completion = completion,
                                 .request = new request_t{
                                     .op = op_t::SEND,
                                     .device = device,
                                     .rank = rank,
                                     .tag = tag,
                                     .buffer = buf,
                                     .length = length,
                                     .user_context = user_context,
                                 }};
  MPI_SAFECALL(MPI_Isend(buf, length, MPI_CHAR, rank, tag,
                         device_p->comm_2sided, &entry.mpi_req));
  add_to_progress_engine(device_p, entry);
  return true;
}

bool backend_mpi_t::recv(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::progress_entry_t entry = {.mpi_req = MPI_REQUEST_NULL,
                                 .completion = completion,
                                 .request = new request_t{
                                     .op = op_t::RECV,
                                     .device = device,
                                     .rank = rank,
                                     .tag = tag,
                                     .buffer = buf,
                                     .length = length,
                                     .user_context = user_context,
                                 }};
  MPI_SAFECALL(MPI_Irecv(buf, length, MPI_CHAR, rank, tag,
                         device_p->comm_2sided, &entry.mpi_req));
  add_to_progress_engine(device_p, entry);
  return true;
}

bool backend_mpi_t::put(device_t device, rank_t rank, void* buf, int64_t length,
                        comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::progress_entry_t entry = {.mpi_req = MPI_REQUEST_NULL,
                                 .completion = completion,
                                 .request = new request_t{
                                     .op = op_t::PUT,
                                     .device = device,
                                     .rank = rank,
                                     .tag = mpi::PUT_SIGNAL_TAG,
                                     .buffer = buf,
                                     .length = length,
                                     .user_context = user_context,
                                 }};
  MPI_SAFECALL(MPI_Isend(entry.request->buffer, entry.request->length, MPI_CHAR,
                         entry.request->rank, entry.request->tag,
                         device_p->comm_1sided, &entry.mpi_req));
  add_to_progress_engine(device_p, entry);
  return true;
}

tag_t backend_mpi_t::get_max_tag(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  return device_p->max_tag;
}

}  // namespace lcw