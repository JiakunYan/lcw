#include "lcwi.hpp"

namespace lcw {
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

int64_t get_rank()
{
  return backend_p->get_rank();
}

int64_t get_nranks()
{
  return backend_p->get_nranks();
}

device_t alloc_device() {
  return backend_p->alloc_device();
}
} // namespace lcw