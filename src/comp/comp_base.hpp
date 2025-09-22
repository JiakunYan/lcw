#include "lcwi.hpp"

namespace lcw
{
namespace util
{
class comp_base_t
{
 public:
  comp_base_t() = default;
  virtual ~comp_base_t() = default;
  virtual void signal(request_t request) = 0;
};

comp_t alloc_cq(int default_cq_length);
void free_cq(comp_t completion);
bool poll_cq(comp_t completion, request_t *request);

comp_t alloc_handler(handler_t handler);
void free_handler(comp_t handler);

static inline void signal_comp(comp_t completion, request_t request)
{
  auto comp = reinterpret_cast<comp_base_t*>(completion);
  comp->signal(request);
}
}  // namespace util
}  // namespace lcw