#include "lcwi.hpp"

namespace lcw
{
namespace util
{
class comp_cq_t : public comp_base_t
{
 public:
  comp_cq_t(size_t default_cq_length)
  {
    cq_ = LCT_queue_alloc(LCT_QUEUE_LCRQ, default_cq_length);
  }
  ~comp_cq_t() { LCT_queue_free(&cq_); }
  void signal(request_t request) override
  {
    auto req = new request_t(request);
    LCT_queue_push(cq_, req);
  }
  bool poll(request_t* request)
  {
    auto* req = static_cast<request_t*>(LCT_queue_pop(cq_));
    if (req == nullptr) return false;
    *request = *req;
    delete req;
    return true;
  }

 private:
  LCT_queue_t cq_;
};

comp_t alloc_cq(int default_cq_length)
{
  comp_base_t* cq = new comp_cq_t(default_cq_length);
  return reinterpret_cast<comp_t>(cq);
}

void free_cq(comp_t completion)
{
  auto cq = reinterpret_cast<comp_base_t*>(completion);
  delete cq;
}

bool poll_cq(comp_t completion, request_t* request)
{
  auto cq = reinterpret_cast<comp_cq_t*>(completion);
  return cq->poll(request);
}
}  // namespace util
}  // namespace lcw