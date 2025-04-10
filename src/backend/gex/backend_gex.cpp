#include <vector>
#include <cstring>
#include <limits>
#include "gasnetex.h"
#include "gasnet_coll.h"
#include "lcwi.hpp"

#define CHECK_GEX(x)                                         \
  {                                                          \
    int err = (x);                                           \
    if (err != GASNET_OK) {                                  \
      printf("err : %d (%s:%d)\n", err, __FILE__, __LINE__); \
      exit(err);                                             \
    }                                                        \
  }                                                          \
  while (0)                                                  \
    ;

namespace lcw
{
namespace gex_detail
{
rank_t rank_me;
rank_t rank_n;
bool finalized;

gex_Client_t client;
gex_EP_t ep;
gex_TM_t tm;
const char* const clientName = "LCW";
const gex_AM_Index_t gex_handler_idx = GEX_AM_INDEX_BASE;
const int GEX_NARGS = 1;

LCT_queue_t cq;

struct device_impl_t {
  int id;
  LCT_queue_t cq;
  device_impl_t() = default;
};

std::vector<device_impl_t*> g_devices;
}  // namespace gex_detail

void gex_reqhandler(gex_Token_t token, void* am_buf, size_t nbytes,
                    gex_AM_Arg_t device_idx)
{
  gex_Token_Info_t info;
  gex_TI_t rc = gex_Token_Info(token, &info, GEX_TI_SRCRANK);
  gex_Rank_t rank = info.gex_srcrank;

  void* buffer = malloc(nbytes);
  memcpy(buffer, am_buf, nbytes);

  auto device = gex_detail::g_devices[device_idx];
  auto* req_p = new request_t;
  *req_p = {
      .op = op_t::PUT_SIGNAL,
      .device = reinterpret_cast<device_t>(device),
      .rank = rank,
      .tag = 0,
      .buffer = buffer,
      .length = static_cast<int64_t>(nbytes),
      .user_context = nullptr,
  };

  LCT_queue_push(device->cq, req_p);
}

void barrier(bool (*do_something)() = nullptr)
{
  gex_Event_t event = gex_Coll_BarrierNB(gex_detail::tm, 0);
  gex_Event_Wait(event);
}

void backend_gex_t::initialize()
{
  gex_Client_Init(&gex_detail::client, &gex_detail::ep, &gex_detail::tm,
                  gex_detail::clientName, NULL, NULL, 0);

#ifndef GASNET_PAR
  throw std::runtime_error("Need to use a par build of GASNet-EX");
#endif

  gex_detail::rank_me = gex_System_QueryJobRank();
  gex_detail::rank_n = gex_System_QueryJobSize();

  gex_detail::finalized = false;

  gex_AM_Entry_t htable[1] = {
      {gex_detail::gex_handler_idx, (gex_AM_Fn_t)gex_reqhandler,
       GEX_FLAG_AM_MEDIUM | GEX_FLAG_AM_REQUEST, gex_detail::GEX_NARGS}};

  CHECK_GEX(gex_EP_RegisterHandlers(gex_detail::ep, htable,
                                    sizeof(htable) / sizeof(gex_AM_Entry_t)));
}

void backend_gex_t::finalize()
{
  barrier();
  gex_detail::finalized = true;
  gasnet_exit(0);
}

int64_t backend_gex_t::get_rank() { return gex_detail::rank_me; }

int64_t backend_gex_t::get_nranks() { return gex_detail::rank_n; }

device_t backend_gex_t::alloc_device(int64_t max_put_length, comp_t put_comp)
{
  auto* device_p = new gex_detail::device_impl_t;
  gex_detail::g_devices.push_back(device_p);
  device_p->id = gex_detail::g_devices.size() - 1;
  device_p->cq = reinterpret_cast<LCT_queue_t>(put_comp);

  auto device = reinterpret_cast<device_t>(device_p);
  return device;
}

void backend_gex_t::free_device(device_t device)
{
  auto device_p = reinterpret_cast<gex_detail::device_impl_t*>(device);
  delete device_p;
}

bool backend_gex_t::do_progress(device_t device)
{
  CHECK_GEX(gasnet_AMPoll());
  return false;
}

comp_t backend_gex_t::alloc_cq()
{
  auto cq = LCT_queue_alloc(LCT_QUEUE_ARRAY_ATOMIC_FAA, 65536);
  return reinterpret_cast<comp_t>(cq);
}

void backend_gex_t::free_cq(comp_t completion)
{
  auto cq = reinterpret_cast<LCT_queue_t>(completion);
  LCT_queue_free(&cq);
}

bool backend_gex_t::poll_cq(comp_t completion, request_t* request)
{
  auto cq = reinterpret_cast<LCT_queue_t>(completion);
  void* ret = LCT_queue_pop(cq);
  if (ret == nullptr) return false;
  auto* req = static_cast<request_t*>(ret);
  *request = *req;
  delete req;
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

bool backend_gex_t::send(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  throw std::runtime_error("send not implemented");
  return false;
}

bool backend_gex_t::recv(device_t device, rank_t rank, tag_t tag, void* buf,
                         int64_t length, comp_t completion, void* user_context)
{
  throw std::runtime_error("recv not implemented");
  return false;
}

bool backend_gex_t::put(device_t device, rank_t rank, void* buf, int64_t length,
                        comp_t completion, void* user_context)
{
  auto device_p = reinterpret_cast<gex_detail::device_impl_t*>(device);
  auto cq = reinterpret_cast<LCT_queue_t>(completion);
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

  CHECK_GEX(gex_AM_RequestMedium1(
      gex_detail::tm, rank, gex_detail::gex_handler_idx, buf, length,
      GEX_EVENT_NOW, 0, static_cast<gex_AM_Arg_t>(device_p->id)));
  LCT_queue_push(cq, req_p);
  return true;
}

tag_t backend_gex_t::get_max_tag(device_t device)
{
  return gex_AM_MaxRequestMedium(gex_detail::tm, GEX_RANK_INVALID,
                                 GEX_EVENT_NOW, 0, gex_detail::GEX_NARGS);
}

}  // namespace lcw