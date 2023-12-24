#ifndef LCW_BACKEND_MPI_HPP
#define LCW_BACKEND_MPI_HPP

#include <mpi.h>

#define MPI_SAFECALL(stmt)                                            \
  do {                                                                \
    int mpi_errno = (stmt);                                           \
    if (MPI_SUCCESS != mpi_errno) {                                   \
      fprintf(stderr, "[%s:%d] MPI call failed with %d \n", __FILE__, \
              __LINE__, mpi_errno);                                   \
      exit(EXIT_FAILURE);                                             \
    }                                                                 \
  } while (0)

namespace lcw {
class backend_mpi_t : public backend_base_t {
  std::string get_name() const override {
    return "mpi";
  }
  backend_t get_backend() const override {
    return backend_t::MPI;
  }
  void initialize() override;
  void finalize() override;
  int64_t get_rank() override;
  int64_t get_nranks() override;
  device_t alloc_device() override;
  void free_device(device_t *device) override;
  bool do_progress(device_t *device) override;
  comp_t alloc_cq() override;
  void free_cq(comp_t completion) override;
  bool poll_cq(comp_t completion, request_t *request) override;
  bool send(device_t device, rank_t rank, tag_t tag,
                    void *buf, int64_t length,
                    comp_t completion, void *user_context) override;
  bool recv(device_t device, rank_t rank, tag_t tag,
                    void *buf, int64_t length,
                    comp_t completion, void *user_context) override;
  bool put(device_t device, rank_t rank,
                   void *buf, int64_t length,
                   comp_t completion, void *user_context) override;
};
} // namespace lcw

#endif  // LCW_BACKEND_MPI_HPP
