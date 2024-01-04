#ifndef LCW_BACKEND_HPP
#define LCW_BACKEND_HPP

namespace lcw
{
class backend_base_t
{
 public:
  virtual ~backend_base_t() = default;
  virtual backend_t get_backend() const = 0;
  virtual std::string get_name() const = 0;
  virtual void initialize() = 0;
  virtual void finalize() = 0;
  virtual int64_t get_rank() = 0;
  virtual int64_t get_nranks() = 0;
  virtual device_t alloc_device(int64_t max_put_length, comp_t put_comp) = 0;
  virtual void free_device(device_t device) = 0;
  virtual bool do_progress(device_t device) = 0;
  virtual comp_t alloc_cq() = 0;
  virtual void free_cq(comp_t completion) = 0;
  virtual bool poll_cq(comp_t completion, request_t* request) = 0;
  virtual bool send(device_t device, rank_t rank, tag_t tag, void* buf,
                    int64_t length, comp_t completion, void* user_context) = 0;
  virtual bool recv(device_t device, rank_t rank, tag_t tag, void* buf,
                    int64_t length, comp_t completion, void* user_context) = 0;
  virtual bool put(device_t device, rank_t rank, void* buf, int64_t length,
                   comp_t completion, void* user_context) = 0;
  virtual tag_t get_max_tag(device_t device) = 0;
};

std::unique_ptr<backend_base_t> alloc_backend(backend_t backend);
}  // namespace lcw

#endif  // LCW_BACKEND_HPP
