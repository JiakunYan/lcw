#ifndef LCW_BACKEND_LCI_HPP
#define LCW_BACKEND_LCI_HPP

namespace lcw
{
class backend_lci_t : public backend_base_t
{
  std::string get_name() const override { return "lci"; }
  backend_t get_backend() const override { return backend_t::LCI; }
  void initialize() override;
  void finalize() override;
  device_t alloc_device(int64_t max_put_length, comp_t put_comp);
  void free_device(device_t device);
  comp_t alloc_cq();
  void free_cq(comp_t completion);
  bool poll_cq(comp_t completion, request_t* request);
  bool send(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
            comp_t completion, void* user_context);
  bool recv(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
            comp_t completion, void* user_context);
  bool put(device_t device, rank_t rank, void* buf, int64_t length,
           comp_t completion, void* user_context);
};
}  // namespace lcw

#endif  // LCW_BACKEND_LCI_HPP
