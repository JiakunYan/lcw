#include <vector>
#include <cstring>
#include "lcwi.hpp"

namespace lcw
{
namespace mpi
{
int g_rank = -1;
int g_nranks = -1;
std::atomic<int> g_pending_msg(0);
config_t config;
std::vector<std::shared_ptr<comp::manager_base_t>> g_comp_managers;
std::atomic<int> g_ndevices(0);
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
#ifndef LCW_MPI_USE_STREAM
    LCW_Assert(mpi::config.comp_type != mpi::config_t::comp_type_t::CONTINUE,
               "MPIX Continue is not enabled!\n");
#endif
    LCW_Log(LCW_LOG_INFO, "comp", "Set LCW_MPI_COMP_TYPE to %d\n",
            mpi::config.comp_type);
  }

  // Use Stream
  {
    char* p = getenv("LCW_MPI_USE_STREAM");
    if (p) {
      mpi::config.use_stream = atoi(p);
    }
#ifndef LCW_MPI_USE_STREAM
    LCW_Assert(!mpi::config.use_stream, "MPIX Stream is not enabled!\n");
#endif
    LCW_Log(LCW_LOG_INFO, "comp", "Set LCW_MPI_USE_STREAM to %d\n",
            mpi::config.use_stream);
  }

  // Max Pending Messages
  {
    char* p = getenv("LCW_MPI_MAX_PENDING_MSG");
    if (p) {
      mpi::config.g_pending_msg_max = atoi(p);
    }
    LCW_Log(LCW_LOG_INFO, "comp", "Set LCW_MPI_MAX_PENDING_MSG to %d\n",
            mpi::config.g_pending_msg_max);
  }

  // Global completion manager number
  {
    char* p = getenv("LCW_MPI_NUM_COMP_MANAGERS");
    if (p) {
      mpi::config.g_num_comp_managers = atoi(p);
    }
    LCW_Log(LCW_LOG_INFO, "comp", "Set LCW_MPI_NUM_COMP_MANAGERS to %d\n",
            mpi::config.g_num_comp_managers);
  }

#ifdef LCW_MPI_USE_CONT
  // Whether to use the immediate flag
  {
    char* p = getenv("LCW_MPI_CONT_IMM");
    if (p) {
      mpi::config.use_cont_imm = atoi(p);
    }
    LCW_Log(LCW_LOG_INFO, "comp", "Set LCW_MPI_CONT_IMM to %d\n",
            mpi::config.use_cont_imm);
  }

  // Whether to use the continuation request (or just nullptr)
  {
    char* p = getenv("LCW_MPI_CONT_REQ");
    if (p) {
      mpi::config.use_cont_req = atoi(p);
    }
    LCW_Log(LCW_LOG_INFO, "comp", "Set LCW_MPI_CONT_REQ to %d\n",
            mpi::config.use_cont_req);
  }
#endif

  if (mpi::config.g_num_comp_managers) {
    mpi::g_comp_managers.resize(mpi::config.g_num_comp_managers);
    for (auto& p : mpi::g_comp_managers) {
      switch (mpi::config.comp_type) {
        case mpi::config_t::comp_type_t::REQUEST:
          p = std::make_shared<mpi::comp::manager_req_t>();
          break;
        case mpi::config_t::comp_type_t::CONTINUE:
#ifdef LCW_MPI_USE_CONT
          p = std::make_shared<mpi::comp::manager_cont_t>();
#else
          LCW_Assert(false, "comp_type cont is not enabled!\n");
#endif
          break;
      }
    }
  }
}

void backend_mpi_t::finalize()
{
  mpi::g_comp_managers.clear();
  MPI_SAFECALL(MPI_Finalize());
}

int64_t backend_mpi_t::get_rank() { return mpi::g_rank; }

int64_t backend_mpi_t::get_nranks() { return mpi::g_nranks; }

void post_put_recv(device_t device, comp_t completion)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::comp::entry_t entry = {
      .mpi_req = MPI_REQUEST_NULL,
      .device = device,
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
  mpi::enter_stream_cs(device);
  MPI_SAFECALL(MPI_Irecv(entry.request->buffer, entry.request->length, MPI_CHAR,
                         MPI_ANY_SOURCE, entry.request->tag, device_p->comm,
                         &entry.mpi_req));
  mpi::leave_stream_cs(device);
  device_p->pengine.put_entry = entry;
}

device_t backend_mpi_t::alloc_device(int64_t max_put_length, comp_t put_comp)
{
  auto* device_p = new mpi::device_t;
  auto device = reinterpret_cast<device_t>(device_p);
  device_p->id = mpi::g_ndevices++;
  // device_p->enable_put = (put_comp != nullptr);
  // Currently, we have to always set this to true to make sure communication
  // progressing.
  device_p->enable_put = true;
  if (mpi::config.use_stream) {
#ifdef LCW_MPI_USE_STREAM
    MPI_SAFECALL(MPIX_Stream_create(MPI_INFO_NULL, &device_p->stream));
    MPI_SAFECALL(MPIX_Stream_comm_create(MPI_COMM_WORLD, device_p->stream,
                                         &device_p->comm));
#endif
  } else {
    MPI_SAFECALL(MPI_Comm_dup(MPI_COMM_WORLD, &device_p->comm));
  }
#if MPI_VERSION >= 3
  MPI_Info hints;
  MPI_Info_create(&hints);
  MPI_Info_set(hints, "mpi_assert_no_any_tag", "true");
  MPI_Info_set(hints, "mpi_assert_allow_overtaking", "true");
  MPI_Comm_set_info(device_p->comm, hints);
#endif
  // get max tag
  int max_tag = 32767;
  void* max_tag_p;
  int flag;
  MPI_Comm_get_attr(device_p->comm, MPI_TAG_UB, &max_tag_p, &flag);
  if (flag) max_tag = *(int*)max_tag_p;
  if (device_p->enable_put) {
    device_p->max_tag_2sided = max_tag - 1;
    device_p->put_tag = max_tag;
  } else {
    device_p->max_tag_2sided = max_tag;
    device_p->put_tag = -1;
  }
  if (!mpi::g_comp_managers.empty()) {
    device_p->pengine.comp_manager_p =
        mpi::g_comp_managers[device_p->id % mpi::g_comp_managers.size()];
  } else {
    switch (mpi::config.comp_type) {
      case mpi::config_t::comp_type_t::REQUEST:
        device_p->pengine.comp_manager_p =
            std::make_shared<mpi::comp::manager_req_t>();
        break;
      case mpi::config_t::comp_type_t::CONTINUE:
#ifdef LCW_MPI_USE_CONT
        device_p->pengine.comp_manager_p =
            std::make_shared<mpi::comp::manager_cont_t>();
#else
        LCW_Assert(false, "comp_type cont is not enabled!\n");
#endif
        break;
    }
  }
  // 1sided
  if (device_p->enable_put) {
    if (max_put_length > 0) {
      device_p->put_rbuf.resize(max_put_length);
    } else {
      device_p->put_rbuf.resize(mpi::config.default_max_put);
    }
    post_put_recv(device, put_comp);
  }
  return device;
}

void backend_mpi_t::free_device(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  MPI_SAFECALL(MPI_Comm_free(&device_p->comm));
#ifdef LCW_MPI_USE_STREAM
  if (mpi::config.use_stream) {
    MPI_SAFECALL(MPIX_Stream_free(&device_p->stream));
  }
#endif
  delete device_p;
}

bool backend_mpi_t::do_progress(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  // work on put
  bool made_progress = false;
  int succeed = 0;
  MPI_Status status;
  mpi::comp::entry_t entry;
  if (device_p->enable_put &&
      device_p->pengine.put_entry.mpi_req != MPI_REQUEST_NULL &&
      device_p->pengine.put_entry_lock.try_lock()) {
    if (device_p->pengine.put_entry.mpi_req != MPI_REQUEST_NULL) {
      mpi::enter_stream_cs(device);
      MPI_SAFECALL(
          MPI_Test(&device_p->pengine.put_entry.mpi_req, &succeed, &status));
      mpi::leave_stream_cs(device);
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
      mpi::push_cq(entry);
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
  if (mpi::config.g_pending_msg_max > 0) {
    if (mpi::g_pending_msg.load(std::memory_order_relaxed) >
        mpi::config.g_pending_msg_max) {
      return false;
    } else {
      mpi::g_pending_msg.fetch_add(1, std::memory_order::memory_order_relaxed);
    }
  }
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::comp::entry_t entry = {.mpi_req = MPI_REQUEST_NULL,
                              .device = device,
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
  mpi::enter_stream_cs(device);
  MPI_SAFECALL(MPI_Isend(buf, length, MPI_CHAR, rank, tag, device_p->comm,
                         &entry.mpi_req));
  mpi::leave_stream_cs(device);
  device_p->pengine.comp_manager_p->add_entry(entry);
  return true;
}

bool backend_mpi_t::recv(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  mpi::comp::entry_t entry = {.mpi_req = MPI_REQUEST_NULL,
                              .device = device,
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
  mpi::enter_stream_cs(device);
  MPI_SAFECALL(MPI_Irecv(buf, length, MPI_CHAR, rank, tag, device_p->comm,
                         &entry.mpi_req));
  mpi::leave_stream_cs(device);
  device_p->pengine.comp_manager_p->add_entry(entry);
  return true;
}

bool backend_mpi_t::put(device_t device, rank_t rank, void* buf, int64_t length,
                        comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  LCW_Assert(device_p->enable_put,
             "Put is not enabled! Please pass the put_comp when allocate the "
             "device\n");
  if (mpi::config.g_pending_msg_max > 0) {
    if (mpi::g_pending_msg.load(std::memory_order_relaxed) >
        mpi::config.g_pending_msg_max) {
      return false;
    } else {
      mpi::g_pending_msg.fetch_add(1, std::memory_order::memory_order_relaxed);
    }
  }
  mpi::comp::entry_t entry = {.mpi_req = MPI_REQUEST_NULL,
                              .device = device,
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
  mpi::enter_stream_cs(device);
  MPI_SAFECALL(MPI_Isend(entry.request->buffer, entry.request->length, MPI_CHAR,
                         entry.request->rank, entry.request->tag,
                         device_p->comm, &entry.mpi_req));
  mpi::leave_stream_cs(device);
  device_p->pengine.comp_manager_p->add_entry(entry);
  return true;
}

tag_t backend_mpi_t::get_max_tag(device_t device)
{
  auto* device_p = reinterpret_cast<mpi::device_t*>(device);
  return device_p->max_tag_2sided;
}

}  // namespace lcw