#include <vector>
#include <cstring>
#include <mpi.h>
#include "lcwi.hpp"
#include "comp_manager/manager_req.hpp"
#include "comp_manager/manager_cont.hpp"

namespace lcw
{
namespace mpi
{
int g_rank = -1;
int g_nranks = -1;

struct config_t {
  LCT_queue_type_t cq_type = LCT_QUEUE_ARRAY_ATOMIC_FAA;
  int default_cq_length = 65536;
  enum class comp_type_t {
    REQUEST,
    CONTINUE,
  } comp_type =
#ifdef LCW_COMP_MANAGER_CONT_ENABLED
      comp_type_t::CONTINUE;
#else
      comp_type_t::REQUEST;
#endif
} config;
}  // namespace mpi

void backend_mpi_t::initialize()
{
  int provided;
  MPI_SAFECALL(
      MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided));
  LCW_Assert(provided == MPI_THREAD_MULTIPLE,
             "Cannot enable multithreaded MPI!\n");
  MPI_SAFECALL(MPI_Comm_rank(MPI_COMM_WORLD, &mpi::g_rank));
  MPI_SAFECALL(MPI_Comm_size(MPI_COMM_WORLD, &mpi::g_nranks));

  // Completion queue type
  {
    LCT_dict_str_int_t dict[] = {
        {NULL, mpi::config.cq_type},
        {"array_atomic_faa", LCT_QUEUE_ARRAY_ATOMIC_FAA},
        {"array_atomic_cas", LCT_QUEUE_ARRAY_ATOMIC_CAS},
        {"array_atomic_basic", LCT_QUEUE_ARRAY_ATOMIC_BASIC},
        {"array_mutex", LCT_QUEUE_ARRAY_MUTEX},
        {"std_mutex", LCT_QUEUE_STD_MUTEX},
    };
    bool succeed = LCT_str_int_search(
        dict, sizeof(dict) / sizeof(dict[0]), getenv("LCW_MPI_CQ_TYPE"),
        mpi::config.cq_type, (int*)&mpi::config.cq_type);
    if (!succeed) {
      LCW_Warn(
          "Unknown LCI_CQ_TYPE %s. Use the default type: array_atomic_faa\n",
          getenv("LCI_CQ_TYPE"));
    }
    LCW_Log(LCW_LOG_INFO, "comp", "Set LCW_MPI_CQ_TYPE to %d\n",
            mpi::config.cq_type);
  }

  // Default completion queue length
  {
    char* p = getenv("LCW_MPI_DEFAULT_QUEUE_LENGTH");
    if (p) {
      mpi::config.default_cq_length = atoi(p);
    }
    LCW_Log(LCW_LOG_INFO, "comp", "Set LCW_MPI_DEFAULT_QUEUE_LENGTH to %d\n",
            mpi::config.default_cq_length);
  }

  // Completion manager type
  {
    LCT_dict_str_int_t dict[] = {
        {NULL, (int)mpi::config.comp_type},
        {"req", (int)mpi::config_t::comp_type_t::REQUEST},
        {"cont", (int)mpi::config_t::comp_type_t::CONTINUE},
    };
    bool succeed = LCT_str_int_search(
        dict, sizeof(dict) / sizeof(dict[0]), getenv("LCW_MPI_COMP_TYPE"),
        (int)mpi::config.comp_type, (int*)&mpi::config.comp_type);
    if (!succeed) {
      LCW_Warn("Unknown LCW_MPI_COMP_TYPE %s\n", getenv("LCW_MPI_COMP_TYPE"));
    }
    LCW_Log(LCW_LOG_INFO, "comp", "Set LCW_MPI_COMP_TYPE to %d\n",
            mpi::config.comp_type);
  }
}

void backend_mpi_t::finalize() { MPI_SAFECALL(MPI_Finalize()); }

int64_t backend_mpi_t::get_rank() { return mpi::g_rank; }

int64_t backend_mpi_t::get_nranks() { return mpi::g_nranks; }

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
  std::unique_ptr<comp::manager_base_t> comp_manager_p;
};

struct device_t {
  MPI_Comm comm;
  std::vector<char> put_rbuf;
  tag_t max_tag_2sided;
  tag_t put_tag;
  progress_engine_t pengine;
};
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
          .tag = device_p->put_tag,
          .buffer = device_p->put_rbuf.data(),
          .length = static_cast<int64_t>(device_p->put_rbuf.size()),
          .user_context = nullptr,
      }};
  MPI_SAFECALL(MPI_Irecv(entry.request->buffer, entry.request->length, MPI_CHAR,
                         MPI_ANY_SOURCE, entry.request->tag, device_p->comm,
                         &entry.mpi_req));
  device_p->pengine.put_entry = entry;
}

device_t backend_mpi_t::alloc_device(int64_t max_put_length, comp_t put_comp)
{
  auto* device_p = new mpi::device_t;
  auto device = reinterpret_cast<device_t>(device_p);
  MPI_SAFECALL(MPI_Comm_dup(MPI_COMM_WORLD, &device_p->comm));
  // get max tag
  int max_tag = 32767;
  void* max_tag_p;
  int flag;
  MPI_Comm_get_attr(device_p->comm, MPI_TAG_UB, &max_tag_p, &flag);
  if (flag) max_tag = *(int*)max_tag_p;
  device_p->max_tag_2sided = max_tag - 1;
  device_p->put_tag = max_tag;
  switch (mpi::config.comp_type) {
    case mpi::config_t::comp_type_t::REQUEST:
      device_p->pengine.comp_manager_p =
          std::make_unique<mpi::comp::manager_req_t>();
      break;
#ifdef LCW_COMP_MANAGER_CONT_ENABLED
    case mpi::config_t::comp_type_t::CONTINUE:
      device_p->pengine.comp_manager_p =
          std::make_unique<mpi::comp::manager_cont_t>();
      break;
#endif
  }
  // 1sided
  if (max_put_length > 0) {
    device_p->put_rbuf.resize(max_put_length);
    post_put_recv(device, put_comp);
  } else {
    device_p->pengine.put_entry.mpi_req = MPI_REQUEST_NULL;
  }
  return device;
}

void backend_mpi_t::free_device(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  MPI_SAFECALL(MPI_Comm_free(&device_p->comm));
  delete device_p;
}

bool backend_mpi_t::do_progress(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  // work on put
  bool made_progress = false;
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
    if (succeed) {
      auto cq = reinterpret_cast<LCT_queue_t>(entry.completion);
      LCT_queue_push(cq, entry.request);
    }
  }
  made_progress = succeed || made_progress;
  made_progress =
      device_p->pengine.comp_manager_p->do_progress() || made_progress;
  return made_progress;
}

comp_t backend_mpi_t::alloc_cq()
{
  LCT_queue_t cq =
      LCT_queue_alloc(mpi::config.cq_type, mpi::config.default_cq_length);
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
  mpi::comp::entry_t entry = {.mpi_req = MPI_REQUEST_NULL,
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
  MPI_SAFECALL(MPI_Isend(buf, length, MPI_CHAR, rank, tag, device_p->comm,
                         &entry.mpi_req));
  device_p->pengine.comp_manager_p->add_entry(entry);
  return true;
}

bool backend_mpi_t::recv(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::comp::entry_t entry = {.mpi_req = MPI_REQUEST_NULL,
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
  MPI_SAFECALL(MPI_Irecv(buf, length, MPI_CHAR, rank, tag, device_p->comm,
                         &entry.mpi_req));
  device_p->pengine.comp_manager_p->add_entry(entry);
  return true;
}

bool backend_mpi_t::put(device_t device, rank_t rank, void* buf, int64_t length,
                        comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::comp::entry_t entry = {.mpi_req = MPI_REQUEST_NULL,
                              .completion = completion,
                              .request = new request_t{
                                  .op = op_t::PUT,
                                  .device = device,
                                  .rank = rank,
                                  .tag = device_p->put_tag,
                                  .buffer = buf,
                                  .length = length,
                                  .user_context = user_context,
                              }};
  MPI_SAFECALL(MPI_Isend(entry.request->buffer, entry.request->length, MPI_CHAR,
                         entry.request->rank, entry.request->tag,
                         device_p->comm, &entry.mpi_req));
  device_p->pengine.comp_manager_p->add_entry(entry);
  return true;
}

tag_t backend_mpi_t::get_max_tag(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  return device_p->max_tag_2sided;
}

}  // namespace lcw