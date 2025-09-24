#ifndef LCW_BACKEND_GEX_HPP
#define LCW_BACKEND_GEX_HPP

namespace lcw
{
class backend_gex_t : public backend_base_t
{
 public:
  backend_gex_t();
  std::string get_name() const override { return "gex"; }
  backend_t get_backend() const override { return backend_t::GEX; }
  void initialize() override;
  void finalize() override;
  int64_t get_rank() override;
  int64_t get_nranks() override;
  device_t alloc_device(int64_t max_put_length, comp_t put_comp) override;
  void free_device(device_t device) override;
  bool do_progress(device_t device) override;
  bool send(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
            comp_t completion, void* user_context);
  bool recv(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
            comp_t completion, void* user_context);
  bool put(device_t device, rank_t rank, void* buf, int64_t length,
           comp_t completion, void* user_context);
  tag_t get_max_tag(device_t device) override;
  void barrier(device_t device) override;
  void set_custom_allocator(allocator_base_t* allocator) override;
};
}  // namespace lcw

#endif  // LCW_BACKEND_GEX_HPP
