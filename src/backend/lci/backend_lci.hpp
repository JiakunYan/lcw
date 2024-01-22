#ifndef LCW_BACKEND_LCI_HPP
#define LCW_BACKEND_LCI_HPP

#define LCI_SAFECALL(stmt)                                                    \
  do {                                                                        \
    int lci_errno = (stmt);                                                   \
    LCW_Assert(LCI_OK == lci_errno, "LCI call failed with %d \n", lci_errno); \
  } while (0)

namespace lcw
{
class backend_lci_t : public backend_base_t
{
  std::string get_name() const override { return "lci"; }
  backend_t get_backend() const override { return backend_t::LCI; }
  void initialize() override;
  void finalize() override;
  int64_t get_rank() override;
  int64_t get_nranks() override;
  device_t alloc_device(int64_t max_put_length, comp_t put_comp) override;
  void free_device(device_t device) override;
  bool do_progress(device_t device) override;
  comp_t alloc_cq();
  void free_cq(comp_t completion);
  bool poll_cq(comp_t completion, request_t* request);
  bool send(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
            comp_t completion, void* user_context);
  bool recv(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
            comp_t completion, void* user_context);
  bool put(device_t device, rank_t rank, void* buf, int64_t length,
           comp_t completion, void* user_context);
  tag_t get_max_tag(device_t device) override;
};
}  // namespace lcw

#endif  // LCW_BACKEND_LCI_HPP
