#ifndef LCW_BACKEND_MPI_HPP
#define LCW_BACKEND_MPI_HPP

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
}  // namespace lcw

#endif  // LCW_BACKEND_MPI_HPP
