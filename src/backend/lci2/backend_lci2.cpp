#include <vector>
#include <cstring>
#include <limits>
#include "lci.hpp"
#include "lcwi.hpp"

namespace lcw
{
void backend_lci2_t::initialize()
{
  lci::g_runtime_init_x().alloc_default_device(false)();
}

void backend_lci2_t::finalize() { lci::g_runtime_fina(); }

int64_t backend_lci2_t::get_rank() { return lci::get_rank(); }

int64_t backend_lci2_t::get_nranks() { return lci::get_nranks(); }

struct device_impl_t {
  int id;
  lci::device_t device;
  lci::rcomp_t rcomp;
  device_impl_t() = default;
};
std::atomic<int> g_ndevices(0);

device_t backend_lci2_t::alloc_device(int64_t max_put_length, comp_t put_comp)
{
  size_t pbuffer_size = lci::get_max_bcopy_size();
  if (max_put_length > pbuffer_size) {
    LCW_Warn(
        "the put length exceeds the eager protocol threshold! (%lu > %lu)\n",
        max_put_length, pbuffer_size);
  }

  auto* device_p = new device_impl_t;
  device_p->id = g_ndevices++;
  lci::comp_t cq(static_cast<void*>(put_comp));
  device_p->rcomp = lci::register_rcomp(cq);
  device_p->device = lci::alloc_device();

  auto device = reinterpret_cast<device_t>(device_p);
  return device;
}

void backend_lci2_t::free_device(device_t device)
{
  auto device_p = reinterpret_cast<device_impl_t*>(device);
  lci::deregister_rcomp(device_p->rcomp);
  lci::free_device(&device_p->device);
}

bool backend_lci2_t::do_progress(device_t device)
{
  auto* device_p = reinterpret_cast<device_impl_t*>(device);
  auto ret = lci::progress_x().device(device_p->device)();
  return ret.is_ok();
}

comp_t backend_lci2_t::alloc_cq()
{
  lci::comp_t cq = lci::alloc_cq();
  return reinterpret_cast<comp_t>(cq.get_impl());
}

void backend_lci2_t::free_cq(comp_t completion)
{
  lci::comp_t cq(static_cast<void*>(completion));
  lci::free_comp(&cq);
}

bool backend_lci2_t::poll_cq(comp_t completion, request_t* request)
{
  lci::comp_t cq(static_cast<void*>(completion));
  lci::status_t status = lci::cq_pop(cq);
  if (status.error.is_retry()) return false;
  lci::buffer_t buffer = status.data.get_buffer();
  if (status.user_context == nullptr) {
    // get PUT_SIGNAL
    *request = {
        .op = op_t::PUT_SIGNAL,
        .device = nullptr,
        .rank = status.rank,
        .tag = static_cast<tag_t>(status.tag),
        .buffer = buffer.base,
        .length = static_cast<int64_t>(buffer.size),
        .user_context = nullptr,
    };
  } else {
    auto* ctx_p = static_cast<request_t*>(status.user_context);
    *request = *ctx_p;
    delete ctx_p;
    if (request->op == op_t::RECV || request->op == op_t::PUT_SIGNAL) {
      request->length = buffer.size;
    }
  }
  switch (request->op) {
    case op_t::SEND:
      pcounter::add(pcounter::send_end);
      break;
    case op_t::RECV:
      pcounter::add(pcounter::recv_end);
      break;
    case op_t::PUT:
      pcounter::add(pcounter::put_end);
      break;
    case op_t::PUT_SIGNAL:
      pcounter::add(pcounter::put_signal);
      break;
  }
  return true;
}

bool backend_lci2_t::send(device_t device, rank_t rank, tag_t tag, void* buf,
                          int64_t length, comp_t completion, void* user_context)
{
  auto device_p = reinterpret_cast<device_impl_t*>(device);
  lci::comp_t cq(static_cast<void*>(completion));
  auto* req_p = new request_t;
  *req_p = {
      .op = op_t::SEND,
      .device = device,
      .rank = rank,
      .tag = tag,
      .buffer = buf,
      .length = length,
      .user_context = user_context,
  };
  lci::status_t status = lci::post_send_x(rank, buf, length, tag, cq)
                             .device(device_p->device)
                             .user_context(req_p)
                             .allow_ok(false)();

  if (status.error.is_retry())
    delete req_p;
  else {
    LCW_DBG_Assert(status.error.is_posted(), "Unexpected return value\n");
  }
  return status.error.is_posted();
}

bool backend_lci2_t::recv(device_t device, rank_t rank, tag_t tag, void* buf,
                          int64_t length, comp_t completion, void* user_context)
{
  auto device_p = reinterpret_cast<device_impl_t*>(device);
  lci::comp_t cq(static_cast<void*>(completion));
  auto* req_p = new request_t;
  *req_p = {
      .op = op_t::RECV,
      .device = device,
      .rank = rank,
      .tag = tag,
      .buffer = buf,
      .length = length,
      .user_context = user_context,
  };
  lci::status_t status = lci::post_recv_x(rank, buf, length, tag, cq)
                             .device(device_p->device)
                             .user_context(req_p)
                             .allow_ok(false)();

  if (status.error.is_retry())
    delete req_p;
  else {
    LCW_DBG_Assert(status.error.is_posted(), "Unexpected return value\n");
  }
  return status.error.is_posted();
}

bool backend_lci2_t::put(device_t device, rank_t rank, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  auto device_p = reinterpret_cast<device_impl_t*>(device);
  lci::comp_t cq(static_cast<void*>(completion));
  auto* req_p = new request_t;
  *req_p = {
      .op = op_t::PUT,
      .device = device,
      .rank = rank,
      .tag = 0,
      .buffer = buf,
      .length = length,
      .user_context = user_context,
  };
  lci::status_t status = lci::post_am_x(rank, buf, length, cq, device_p->rcomp)
                             .device(device_p->device)
                             .user_context(req_p)
                             .allow_ok(false)();

  if (status.error.is_retry())
    delete req_p;
  else {
    LCW_DBG_Assert(status.error.is_posted(), "Unexpected return value\n");
  }
  return status.error.is_posted();
}

tag_t backend_lci2_t::get_max_tag(device_t device)
{
  return lci::get_g_runtime().get_attr_max_imm_tag();
}

}  // namespace lcw