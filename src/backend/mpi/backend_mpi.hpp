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
};
extern config_t config;

struct progress_engine_t {
  comp::entry_t put_entry;
  spinlock_t put_entry_lock;
  std::unique_ptr<comp::manager_base_t> comp_manager_p;
};

struct device_t {
#ifdef LCW_MPI_USE_STREAM
  spinlock_t stream_lock;
  MPIX_Stream stream;
#endif
  MPI_Comm comm;
  std::vector<char> put_rbuf;
  tag_t max_tag_2sided;
  tag_t put_tag;
  progress_engine_t pengine;
};

static inline void enter_stream_cs(lcw::device_t device)
{
  if (config.use_stream) {
    auto* device_p = reinterpret_cast<mpi::device_t*>(device);
    device_p->stream_lock.lock();
  }
}

static inline void leave_stream_cs(lcw::device_t device)
{
  if (config.use_stream) {
    auto* device_p = reinterpret_cast<mpi::device_t*>(device);
    device_p->stream_lock.unlock();
  }
}
}  // namespace mpi
}  // namespace lcw

#endif  // LCW_BACKEND_MPI_HPP
