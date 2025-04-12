#include <vector>
#include <cstring>
#include "lci.h"
#include "lcwi.hpp"

namespace lcw
{
void backend_lci_t::initialize() { LCI_SAFECALL(LCI_initialize()); }

void backend_lci_t::finalize() { LCI_SAFECALL(LCI_finalize()); }

int64_t backend_lci_t::get_rank() { return LCI_RANK; }

int64_t backend_lci_t::get_nranks() { return LCI_NUM_PROCESSES; }

namespace backend_lci
{
struct device_t {
  int id;
  LCI_device_t device;
  LCI_endpoint_t ep;
};
std::atomic<int> g_ndevices(0);
}  // namespace backend_lci

device_t backend_lci_t::alloc_device(int64_t max_put_length, comp_t put_comp)
{
  LCW_Assert(max_put_length <= LCI_MEDIUM_SIZE,
             "the put length is too large!\n");
  auto* device_p = new backend_lci::device_t;
  device_p->id = backend_lci::g_ndevices++;
  if (device_p->id == 0) {
    device_p->device = LCI_UR_DEVICE;
  } else {
    LCI_SAFECALL(LCI_device_init(&device_p->device));
  }
  auto cq = reinterpret_cast<LCI_comp_t>(put_comp);
  LCI_plist_t plist;
  LCI_SAFECALL(LCI_plist_create(&plist));
  LCI_SAFECALL(
      LCI_plist_set_comp_type(plist, LCI_PORT_COMMAND, LCI_COMPLETION_QUEUE));
  LCI_SAFECALL(
      LCI_plist_set_comp_type(plist, LCI_PORT_MESSAGE, LCI_COMPLETION_QUEUE));
  LCI_SAFECALL(LCI_plist_set_match_type(plist, LCI_MATCH_RANKTAG));
  LCI_SAFECALL(LCI_plist_set_default_comp(plist, cq));
  LCI_SAFECALL(LCI_endpoint_init(&device_p->ep, device_p->device, plist));
  LCI_SAFECALL(LCI_plist_free(&plist));

  auto device = reinterpret_cast<device_t>(device_p);
  return device;
}

void backend_lci_t::free_device(device_t device)
{
  auto* device_p = reinterpret_cast<backend_lci::device_t*>(device);
  LCI_SAFECALL(LCI_endpoint_free(&device_p->ep));
  if (device_p->id != 0) LCI_SAFECALL(LCI_device_free(&device_p->device));
}

bool backend_lci_t::do_progress(device_t device)
{
  auto* device_p = reinterpret_cast<backend_lci::device_t*>(device);
  LCI_progress(device_p->device);
  return false;
}

comp_t backend_lci_t::alloc_cq()
{
  LCI_comp_t cq;
  LCI_queue_create(nullptr, &cq);
  return reinterpret_cast<comp_t>(cq);
}

void backend_lci_t::free_cq(comp_t completion)
{
  auto cq = reinterpret_cast<LCI_comp_t>(completion);
  LCI_queue_free(&cq);
}

bool backend_lci_t::poll_cq(comp_t completion, request_t* request)
{
  auto cq = reinterpret_cast<LCI_comp_t>(completion);
  LCI_request_t lci_req;
  LCI_error_t ret = LCI_queue_pop(cq, &lci_req);
  if (ret == LCI_ERR_RETRY) return false;
  if (lci_req.user_context == nullptr) {
    // get PUT_SIGNAL
    void* buffer;
    int mem_ret =
        posix_memalign(&buffer, LCW_CACHE_LINE, lci_req.data.mbuffer.length);
    LCW_Assert(mem_ret == 0, "posix_memalign(%ld) failed!\n", request->length);
    memcpy(buffer, lci_req.data.mbuffer.address, lci_req.data.mbuffer.length);
    *request = {
        .op = op_t::PUT_SIGNAL,
        .device = nullptr,
        .rank = lci_req.rank,
        .tag = lci_req.tag,
        .buffer = buffer,
        .length = static_cast<int64_t>(lci_req.data.mbuffer.length),
        .user_context = nullptr,
    };
    LCI_mbuffer_free(lci_req.data.mbuffer);
  } else {
    auto* ctx_p = static_cast<request_t*>(lci_req.user_context);
    *request = *ctx_p;
    delete ctx_p;
    if (request->op == op_t::RECV || request->op == op_t::PUT_SIGNAL) {
      if (lci_req.type == LCI_MEDIUM)
        request->length = static_cast<int64_t>(lci_req.data.mbuffer.length);
      else
        request->length = static_cast<int64_t>(lci_req.data.lbuffer.length);
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

bool backend_lci_t::send(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<backend_lci::device_t*>(device);
  auto cq = static_cast<LCI_comp_t>(completion);
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
  LCI_error_t ret;
  if (length <= LCI_MEDIUM_SIZE) {
    LCI_mbuffer_t mbuffer = {
        .address = buf,
        .length = static_cast<size_t>(length),
    };
    ret = LCI_sendmc(device_p->ep, mbuffer, static_cast<int>(rank), tag, cq,
                     req_p);
  } else {
    LCI_lbuffer_t lbuffer = {
        .segment = LCI_SEGMENT_ALL,
        .address = buf,
        .length = static_cast<size_t>(length),
    };
    ret = LCI_sendl(device_p->ep, lbuffer, static_cast<int>(rank), tag, cq,
                    req_p);
  }
  LCW_DBG_Assert(ret == LCI_OK || ret == LCI_ERR_RETRY,
                 "Unexpected return value %d\n", ret);
  if (ret != LCI_OK)
    delete req_p;
  else {
    LCW_DBG_Assert(ret == LCI_OK, "Unexpected return value %d\n", ret);
  }
  return ret == LCI_OK;
}

bool backend_lci_t::recv(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<backend_lci::device_t*>(device);
  auto cq = static_cast<LCI_comp_t>(completion);
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
  LCI_error_t ret;
  if (length <= LCI_MEDIUM_SIZE) {
    LCI_mbuffer_t mbuffer = {
        .address = buf,
        .length = static_cast<size_t>(length),
    };
    ret = LCI_recvm(device_p->ep, mbuffer, static_cast<int>(rank), tag, cq,
                    req_p);
  } else {
    LCI_lbuffer_t lbuffer = {
        .segment = LCI_SEGMENT_ALL,
        .address = buf,
        .length = static_cast<size_t>(length),
    };
    ret = LCI_recvl(device_p->ep, lbuffer, static_cast<int>(rank), tag, cq,
                    req_p);
  }
  if (ret != LCI_OK)
    delete req_p;
  else {
    LCW_DBG_Assert(ret == LCI_OK, "Unexpected return value %d\n", ret);
  }
  return ret == LCI_OK;
}

bool backend_lci_t::put(device_t device, rank_t rank, void* buf, int64_t length,
                        comp_t completion, void* user_context)
{
  auto* device_p = reinterpret_cast<backend_lci::device_t*>(device);
  auto cq = static_cast<LCI_comp_t>(completion);
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
  LCI_error_t ret;
  if (length <= LCI_MEDIUM_SIZE) {
    LCI_mbuffer_t mbuffer = {
        .address = buf,
        .length = static_cast<size_t>(length),
    };
    ret = LCI_putmac(device_p->ep, mbuffer, static_cast<int>(rank), 0,
                     LCI_DEFAULT_COMP_REMOTE, cq, req_p);
  } else {
    LCI_lbuffer_t lbuffer = {
        .segment = LCI_SEGMENT_ALL,
        .address = buf,
        .length = static_cast<size_t>(length),
    };
    ret = LCI_putla(device_p->ep, lbuffer, cq, static_cast<int>(rank), 0,
                    LCI_DEFAULT_COMP_REMOTE, req_p);
  }
  LCW_DBG_Assert(ret == LCI_OK || ret == LCI_ERR_RETRY,
                 "Unexpected return value %d\n", ret);
  if (ret != LCI_OK)
    delete req_p;
  else {
    LCW_DBG_Assert(ret == LCI_OK, "Unexpected return value %d\n", ret);
  }
  return ret == LCI_OK;
}

tag_t backend_lci_t::get_max_tag(device_t device) { return LCI_MAX_TAG; }

void backend_lci_t::barrier(device_t device)
{
  auto* device_p = reinterpret_cast<backend_lci::device_t*>(device);
  LCI_SAFECALL(LCI_barrier());
}

}  // namespace lcw