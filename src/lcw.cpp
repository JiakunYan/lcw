#include "lcwi.hpp"

namespace lcw
{
std::unique_ptr<backend_base_t> backend_p(nullptr);

void init_env();

void initialize(backend_t backend)
{
  if (backend_p) {
    fprintf(stderr, "LCW has not been initialized or has been finalized!\n");
    abort();
  }
  LCT_init();
  LCWI_log_init();
  custom_spinlock_init();
  init_env();
  backend_p = alloc_backend(backend);
  LCW_Log(LCW_LOG_TRACE, "init", "Start initializing\n");
  backend_p->initialize();
  LCT_set_rank(static_cast<int>(get_rank()));
  pcounter::initialize();
  LCW_Log(LCW_LOG_TRACE, "init", "Initialize successfully\n");
}

void finalize()
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  backend_p->finalize();
  backend_p.reset(nullptr);
  pcounter::finalize();
  LCWI_log_fina();
  LCT_fina();
}

bool is_initialized() { return backend_p != nullptr; }

int64_t get_rank()
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  return backend_p->get_rank();
}

int64_t get_nranks()
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  return backend_p->get_nranks();
}

device_t alloc_device(int64_t max_put_length, comp_t put_comp)
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  return backend_p->alloc_device(max_put_length, put_comp);
}

void free_device(device_t device)
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  backend_p->free_device(device);
}

bool do_progress(device_t device)
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  pcounter::add(pcounter::progress);
  return backend_p->do_progress(device);
}

comp_t alloc_cq()
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  return backend_p->alloc_cq();
}

void free_cq(comp_t completion)
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  backend_p->free_cq(completion);
}

bool poll_cq(comp_t completion, request_t* request)
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  pcounter::add(pcounter::comp_poll);
  bool ret = backend_p->poll_cq(completion, request);
  if (ret) {
    LCW_Log(LCW_log_level_t::LCW_LOG_TRACE, "comm",
            "poll_cq(%p, {%d, %p, %ld, %ld, %p, %ld, %p})\n", completion,
            request->op, request->device, request->rank, request->tag,
            request->buffer, request->length, request->user_context);
    pcounter::add(pcounter::comp_consume);
  }
  return ret;
}

bool send(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
          comp_t completion, void* user_context)
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  LCW_Log(LCW_log_level_t::LCW_LOG_TRACE, "comm",
          "send(%p, %ld, %ld, %p, %ld, %p, %p)\n", device, rank, tag, buf,
          length, completion, user_context);
  pcounter::start(pcounter::send_timer);
  bool ret =
      backend_p->send(device, rank, tag, buf, length, completion, user_context);
  pcounter::end(pcounter::send_timer);
  if (ret) {
    pcounter::add(pcounter::send_start);
  } else {
    pcounter::add(pcounter::send_retry);
  }
  return ret;
}

bool recv(device_t device, rank_t rank, tag_t tag, void* buf, int64_t length,
          comp_t completion, void* user_context)
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  LCW_Log(LCW_log_level_t::LCW_LOG_TRACE, "comm",
          "recv(%p, %ld, %ld, %p, %ld, %p, %p)\n", device, rank, tag, buf,
          length, completion, user_context);
  pcounter::start(pcounter::recv_timer);
  bool ret =
      backend_p->recv(device, rank, tag, buf, length, completion, user_context);
  pcounter::end(pcounter::recv_timer);
  if (ret) {
    pcounter::add(pcounter::recv_start);
  } else {
    pcounter::add(pcounter::recv_retry);
  }
  return ret;
}

bool put(device_t device, rank_t rank, void* buf, int64_t length,
         comp_t completion, void* user_context)
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  LCW_Log(LCW_log_level_t::LCW_LOG_TRACE, "comm",
          "put(%p, %ld, %p, %ld, %p, %p)\n", device, rank, buf, length,
          completion, user_context);
  pcounter::start(pcounter::put_timer);
  bool ret =
      backend_p->put(device, rank, buf, length, completion, user_context);
  pcounter::end(pcounter::put_timer);
  if (ret) {
    pcounter::add(pcounter::put_start);
  } else {
    pcounter::add(pcounter::put_retry);
  }
  return ret;
}

tag_t get_max_tag(device_t device)
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  return backend_p->get_max_tag(device);
}

void barrier(device_t device)
{
  LCW_Assert(backend_p != nullptr,
             "LCW has not been initialized or has been finalized!\n");
  backend_p->barrier(device);
}
}  // namespace lcw