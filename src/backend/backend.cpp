#include "lcwi.hpp"

namespace lcw
{
std::unique_ptr<backend_base_t> alloc_backend(backend_t backend)
{
  if (backend == backend_t::AUTO) {
    // Default auto backend is MPI.
    backend =
#ifdef LCW_ENABLE_BACKEND_MPI
        backend_t::MPI;
#elif defined(LCW_ENABLE_BACKEND_LCI)
        backend_t::LCI;
#else
        backend_t::LCI2;
#endif
    // Check env var setting
    char* p = getenv("LCW_BACKEND_AUTO");
    if (p) {
      std::string str(p);
      if (str == "mpi") {
        backend = backend_t::MPI;
      } else if (str == "lci") {
        backend = backend_t::LCI;
      } else if (str == "lci2") {
        backend = backend_t::LCI2;
      }
    }
  }
  switch (backend) {
    case backend_t::LCI:
#ifdef LCW_ENABLE_BACKEND_LCI
      return std::make_unique<backend_lci_t>();
#else
      LCW_Assert(false, "The LCI backend is not available\n");
#endif
    case backend_t::MPI:
#ifdef LCW_ENABLE_BACKEND_MPI
      return std::make_unique<backend_mpi_t>();
#else
      LCW_Assert(false, "The MPI backend is not available\n");
#endif
    case backend_t::LCI2:
#ifdef LCW_ENABLE_BACKEND_LCI2
      return std::make_unique<backend_lci2_t>();
#else
      LCW_Assert(false, "The LCI2 backend is not available\n");
#endif
    default:
      LCW_Assert(false, "Unknown backend %d\n", backend);
      return nullptr;
  }
}
}  // namespace lcw