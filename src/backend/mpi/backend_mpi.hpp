#ifndef LCW_BACKEND_MPI_HPP
#define LCW_BACKEND_MPI_HPP
#include <mpi.h>

#ifdef MPIX_STREAM_NULL
#define LCW_MPI_USE_STREAM
#endif
#ifdef MPIX_CONT_POLL_ONLY
#define LCW_MPI_USE_CONT
#endif

#define MPI_SAFECALL(stmt)                                             \
  do {                                                                 \
    int mpi_errno = (stmt);                                            \
    LCW_Assert(MPI_SUCCESS == mpi_errno, "MPI call failed with %d \n", \
               mpi_errno);                                             \
  } while (0)

namespace lcw
{
class backend_mpi_t : public backend_base_t
{
  std::string get_name() const override { return "mpi"; }
  backend_t get_backend() const override { return backend_t::MPI; }
  void initialize() override;
  void finalize() override;
  int64_t get_rank() override;
  int64_t get_nranks() override;
  device_t alloc_device(int64_t max_put_length, comp_t put_comp) override;
  void free_device(device_t device) override;
  bool do_progress(device_t device) override;
  comp_t alloc_cq() override;
  void free_cq(comp_t completion) override;
  bool poll_cq(comp_t completion, request_t* request) override;
  bool send(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
            comp_t completion, void* user_context) override;
  bool recv(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
            comp_t completion, void* user_context) override;
  bool put(device_t device, rank_t rank, void* buf, int64_t length,
           comp_t completion, void* user_context) override;
  tag_t get_max_tag(device_t device) override;
};

namespace mpi
{
struct config_t {
  LCT_queue_type_t cq_type = LCT_QUEUE_ARRAY_ATOMIC_FAA;
  int default_max_put = 8192;
  int default_cq_length = 65536;
  enum class comp_type_t {
    REQUEST,
    CONTINUE,
  } comp_type =
#ifdef LCW_MPI_USE_CONT
      comp_type_t::CONTINUE;
#else
      comp_type_t::REQUEST;
#endif
  bool use_stream = false;
#ifdef LCW_MPI_USE_CONT
  bool use_cont_imm = true;
  bool use_cont_req = false;
#endif
  int g_pending_msg_max = 256;
  int g_num_comp_managers = 0;
};
extern config_t config;
extern std::atomic<int> g_pending_msg;
extern std::vector<std::shared_ptr<comp::manager_base_t>> g_comp_managers;
extern std::atomic<int> g_ndevices;

struct progress_engine_t {
  comp::entry_t put_entry;
  spinlock_t put_entry_lock;
  std::shared_ptr<comp::manager_base_t> comp_manager_p;
};

struct device_t {
#ifdef LCW_MPI_USE_STREAM
  spinlock_t stream_lock;
  MPIX_Stream stream;
#endif
  int id;
  bool enable_put;
  MPI_Comm comm;
  std::vector<char> put_rbuf;
  tag_t max_tag_2sided;
  tag_t put_tag;
  progress_engine_t pengine;
};

static inline void enter_stream_cs(lcw::device_t device)
{
#ifdef LCW_MPI_USE_STREAM
  if (config.use_stream) {
    auto* device_p = reinterpret_cast<mpi::device_t*>(device);
    device_p->stream_lock.lock();
  }
#endif
}

static inline void leave_stream_cs(lcw::device_t device)
{
#ifdef LCW_MPI_USE_STREAM
  if (config.use_stream) {
    auto* device_p = reinterpret_cast<mpi::device_t*>(device);
    device_p->stream_lock.unlock();
  }
#endif
}

static void push_cq(const comp::entry_t& entry)
{
  auto cq = reinterpret_cast<LCT_queue_t>(entry.completion);
  switch (entry.request->op) {
    case op_t::SEND:
      if (config.g_pending_msg_max > 0)
        g_pending_msg.fetch_sub(1, std::memory_order::memory_order_relaxed);
      pcounter::add(pcounter::send_end);
      break;
    case op_t::RECV:
      pcounter::add(pcounter::recv_end);
      break;
    case op_t::PUT:
      if (config.g_pending_msg_max > 0)
        g_pending_msg.fetch_sub(1, std::memory_order::memory_order_relaxed);
      pcounter::add(pcounter::put_end);
      break;
    case op_t::PUT_SIGNAL:
      pcounter::add(pcounter::put_signal);
      break;
  }
  pcounter::add(pcounter::comp_produce);
  LCT_queue_push(cq, entry.request);
}
}  // namespace mpi
}  // namespace lcw

#endif  // LCW_BACKEND_MPI_HPP
