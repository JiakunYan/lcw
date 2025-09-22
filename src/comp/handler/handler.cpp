#include "lcwi.hpp"

namespace lcw
{
namespace util
{
class comp_handler_t : public comp_base_t
{
 public:
  comp_handler_t(handler_t handler) : handler_(handler) {}
  ~comp_handler_t() = default;
  void signal(request_t request) override { handler_(request); }

 private:
  handler_t handler_;
};

comp_t alloc_handler(handler_t handler)
{
  comp_base_t* h = new comp_handler_t(handler);
  return reinterpret_cast<comp_t>(h);
}

void free_handler(comp_t handler)
{
  auto h = reinterpret_cast<comp_base_t*>(handler);
  delete h;
}

}  // namespace util
}  // namespace lcw