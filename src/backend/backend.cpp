#include "lcwi.hpp"

namespace lcw
{
std::unique_ptr<backend_base_t> alloc_backend(backend_t backend)
{
  switch (backend) {
    case backend_t::LCI:
#ifdef LCW_ENABLE_BACKEND_LCI
      return std::make_unique<backend_lci_t>();
#else
      LCW_Assert(false, "The LCI backend is not available\n");
#endif
#ifdef LCW_ENABLE_BACKEND_MPI
    case backend_t::MPI:
      return std::make_unique<backend_mpi_t>();
#else
      LCW_Assert(false, "The MPI backend is not available\n");
#endif
    default:
      LCW_Assert(false, "Unknown backend %d\n", backend);
      return nullptr;
  }
}
}  // namespace lcw