#include "lcwi.hpp"

namespace lcw
{
std::unique_ptr<backend_base_t> alloc_backend(backend_t backend)
{
  if (backend == backend_t::AUTO) {
    // Default auto backend is MPI.
    backend =
#ifdef LCW_ENABLE_BACKEND_LCI2
        backend_t::LCI2;
#elif defined(LCW_ENABLE_BACKEND_LCI)
        backend_t::LCI;
#else
        backend_t::MPI;
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
      } else if (str == "gex") {
        backend = backend_t::GEX;
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
    case backend_t::GEX:
#ifdef LCW_ENABLE_BACKEND_GEX
      return std::make_unique<backend_gex_t>();
#else
      LCW_Assert(false, "The GEX backend is not available\n");
#endif
    default:
      LCW_Assert(false, "Unknown backend %d\n", backend);
      return nullptr;
  }
}

comp_t backend_base_t::alloc_handler(handler_t handler)
{
  return util::alloc_handler(handler);
}

void backend_base_t::free_handler(comp_t handler)
{
  util::free_handler(handler);
}

comp_t backend_base_t::alloc_cq()
{
  return util::alloc_cq(mpi::config.default_cq_length);
}

void backend_base_t::free_cq(comp_t completion) { util::free_cq(completion); }

bool backend_base_t::poll_cq(comp_t completion, request_t* request)
{
  return util::poll_cq(completion, request);
}
}  // namespace lcw