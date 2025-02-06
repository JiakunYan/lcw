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
  lci::net_device_t device;
  lci::net_endpoint_t ep;
  lci::rcomp_t rcomp;
  device_impl_t() = default;
};
std::atomic<int> g_ndevices(0);

device_t backend_lci2_t::alloc_device(int64_t max_put_length, comp_t put_comp)
{
  size_t pbuffer_size = lci::get_default_packet_pool().get_attr_pbuffer_size();
  if (max_put_length <= pbuffer_size) {
    LCW_Warn("the put length exceeds the eager protocol threshold!\n");
  }
  auto* device_p = new device_impl_t;
  device_p->id = g_ndevices++;
  device_p->device = lci::alloc_net_device();
  device_p->ep = lci::alloc_net_endpoint_x().net_device(device_p->device)();
  lci::comp_t cq(static_cast<void*>(put_comp));
  device_p->rcomp = lci::register_rcomp(cq);

  auto device = reinterpret_cast<device_t>(device_p);
  return device;
}

void backend_lci2_t::free_device(device_t device)
{
  auto device_p = reinterpret_cast<device_impl_t*>(device);
  lci::deregister_rcomp(device_p->rcomp);
  lci::free_net_endpoint(&device_p->ep);
  lci::free_net_device(&device_p->device);
}

bool backend_lci2_t::do_progress(device_t device)
{
  auto* device_p = reinterpret_cast<device_impl_t*>(device);
  auto ret = lci::progress_x().net_device(device_p->device)();
  return ret.is_ok();
}

comp_t backend_lci2_t::alloc_cq()
{
  lci::comp_t cq = lci::alloc_cq();
  return static_cast<comp_t>(cq.get_p());
}

void backend_lci2_t::free_cq(comp_t completion)
{
  lci::comp_t cq(static_cast<void*>(completion));
  lci::free_cq(&cq);
}

bool backend_lci2_t::poll_cq(comp_t completion, request_t* request)
{
  lci::comp_t cq(static_cast<void*>(completion));
  lci::status_t status = lci::cq_pop(cq);
  if (status.error.is_retry()) return false;
  if (status.user_context == nullptr) {
    // get PUT_SIGNAL
    *request = {
        .op = op_t::PUT_SIGNAL,
        .device = nullptr,
        .rank = status.rank,
        .tag = status.tag,
        .buffer = status.buffer,
        .length = static_cast<int64_t>(status.size),
        .user_context = nullptr,
    };
  } else {
    auto* ctx_p = static_cast<request_t*>(status.user_context);
    *request = *ctx_p;
    delete ctx_p;
    if (request->op == op_t::RECV || request->op == op_t::PUT_SIGNAL) {
      request->length = status.size;
    }
  }
  return true;
}

bool backend_lci2_t::send(device_t device, rank_t rank, tag_t tag, void* buf,
                          int64_t length, comp_t completion, void* user_context)
{
  throw std::runtime_error("not implemented");
  // auto* device_p = static_cast<device_impl_t*>(device);
  // auto cq = static_cast<LCI_comp_t>(completion);
  // auto* req_p = new request_t;
  // *req_p = {
  //     .op = op_t::SEND,
  //     .device = device,
  //     .rank = rank,
  //     .tag = tag,
  //     .buffer = buf,
  //     .length = length,
  //     .user_context = user_context,
  // };
  // LCI_error_t ret;
  // if (length <= LCI_MEDIUM_SIZE) {
  //   LCI_mbuffer_t mbuffer = {
  //       .address = buf,
  //       .length = static_cast<size_t>(length),
  //   };
  //   ret = LCI_sendmc(device_p->ep, mbuffer, static_cast<int>(rank), tag, cq,
  //                    req_p);
  // } else {
  //   LCI_lbuffer_t lbuffer = {
  //       .segment = LCI_SEGMENT_ALL,
  //       .address = buf,
  //       .length = static_cast<size_t>(length),
  //   };
  //   ret = LCI_sendl(device_p->ep, lbuffer, static_cast<int>(rank), tag, cq,
  //                   req_p);
  // }
  // LCW_DBG_Assert(ret == LCI_OK || ret == LCI_ERR_RETRY,
  //                "Unexpected return value %d\n", ret);
  // if (ret != LCI_OK)
  //   delete req_p;
  // else {
  //   LCW_DBG_Assert(ret == LCI_OK, "Unexpected return value %d\n", ret);
  // }
  // return ret == LCI_OK;
}

bool backend_lci2_t::recv(device_t device, rank_t rank, tag_t tag, void* buf,
                          int64_t length, comp_t completion, void* user_context)
{
  throw std::runtime_error("not implemented");
  // auto* device_p = static_cast<device_impl_t*>(device);
  // auto cq = static_cast<LCI_comp_t>(completion);
  // auto* req_p = new request_t;
  // *req_p = {
  //     .op = op_t::RECV,
  //     .device = device,
  //     .rank = rank,
  //     .tag = tag,
  //     .buffer = buf,
  //     .length = length,
  //     .user_context = user_context,
  // };
  // LCI_error_t ret;
  // if (length <= LCI_MEDIUM_SIZE) {
  //   LCI_mbuffer_t mbuffer = {
  //       .address = buf,
  //       .length = static_cast<size_t>(length),
  //   };
  //   ret = LCI_recvm(device_p->ep, mbuffer, static_cast<int>(rank), tag, cq,
  //                   req_p);
  // } else {
  //   LCI_lbuffer_t lbuffer = {
  //       .segment = LCI_SEGMENT_ALL,
  //       .address = buf,
  //       .length = static_cast<size_t>(length),
  //   };
  //   ret = LCI_recvl(device_p->ep, lbuffer, static_cast<int>(rank), tag, cq,
  //                   req_p);
  // }
  // if (ret != LCI_OK)
  //   delete req_p;
  // else {
  //   LCW_DBG_Assert(ret == LCI_OK, "Unexpected return value %d\n", ret);
  // }
  // return ret == LCI_OK;
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
                             .net_endpoint(device_p->ep)
                             .ctx(req_p)
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
  return lci::get_g_default_runtime().get_attr_max_imm_tag();
}

}  // namespace lcw