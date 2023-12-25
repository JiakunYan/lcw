#include "lcwi.hpp"

namespace lcw
{
std::unique_ptr<backend_base_t> backend_p;

void initialize(backend_t backend)
{
  LCT_init();
  LCWI_log_init();
  backend_p = alloc_backend(backend);
  backend_p->initialize();
}

void finalize()
{
  backend_p->finalize();
  backend_p.reset(nullptr);
  LCWI_log_fina();
  LCT_fina();
}

int64_t get_rank() { return backend_p->get_rank(); }

int64_t get_nranks() { return backend_p->get_nranks(); }

device_t alloc_device() { return backend_p->alloc_device(); }

void free_device(device_t* device) { backend_p->free_device(device); }

bool do_progress(device_t device) { return backend_p->do_progress(device); }

comp_t alloc_cq() { return backend_p->alloc_cq(); }

void free_cq(comp_t completion) { backend_p->free_cq(completion); }

bool poll_cq(comp_t completion, request_t* request)
{
  return backend_p->poll_cq(completion, request);
}

bool send(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
          comp_t completion, void* user_context)
{
  return backend_p->send(device, rank, tag, buf, length, completion,
                         user_context);
}

bool recv(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
          comp_t completion, void* user_context)
{
  return backend_p->recv(device, rank, tag, buf, length, completion,
                         user_context);
}

bool put(device_t device, rank_t rank, void* buf, int64_t length,
         comp_t completion, void* user_context)
{
  return backend_p->put(device, rank, buf, length, completion, user_context);
}
}  // namespace lcw