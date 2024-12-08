#include "lcwi.hpp"

namespace lcw
{
namespace pcounter
{
LCT_pcounter_ctx_t pcounter_ctx;

#define LCW_PCOUNTER_HANDLE_DEF(name) LCT_pcounter_handle_t name;
LCW_PCOUNTER_NONE_FOR_EACH(LCW_PCOUNTER_HANDLE_DEF)
LCW_PCOUNTER_TREND_FOR_EACH(LCW_PCOUNTER_HANDLE_DEF)
LCW_PCOUNTER_TIMER_FOR_EACH(LCW_PCOUNTER_HANDLE_DEF)

void initialize()
{
  // initialize the performance counters
#ifdef LCW_ENABLE_PCOUNTER
  pcounter_ctx = LCT_pcounter_ctx_alloc("lcw");

#define LCW_PCOUNTER_NONE_REGISTER(name) \
  name = LCT_pcounter_register(pcounter_ctx, #name, LCT_PCOUNTER_NONE);
  LCW_PCOUNTER_NONE_FOR_EACH(LCW_PCOUNTER_NONE_REGISTER)

#define LCW_PCOUNTER_TREND_REGISTER(name) \
  name = LCT_pcounter_register(pcounter_ctx, #name, LCT_PCOUNTER_TREND);
  LCW_PCOUNTER_TREND_FOR_EACH(LCW_PCOUNTER_TREND_REGISTER)

#define LCW_PCOUNTER_TIMER_REGISTER(name) \
  name = LCT_pcounter_register(pcounter_ctx, #name, LCT_PCOUNTER_TIMER);
  LCW_PCOUNTER_TIMER_FOR_EACH(LCW_PCOUNTER_TIMER_REGISTER)
#endif
}

void finalize()
{
#ifdef LCW_ENABLE_PCOUNTER
  LCT_pcounter_ctx_free(&pcounter_ctx);
#endif
}

int64_t now()
{
#ifdef LCW_ENABLE_PCOUNTER
  return static_cast<int64_t>(LCT_now());
#endif
  return 0;
}

int64_t since([[maybe_unused]] int64_t then)
{
#ifdef LCW_ENABLE_PCOUNTER
  return static_cast<int64_t>(LCT_now()) - then;
#endif
  return 0;
}

void add([[maybe_unused]] LCT_pcounter_handle_t handle,
         [[maybe_unused]] int64_t val)
{
#ifdef LCW_ENABLE_PCOUNTER
  LCT_pcounter_add(pcounter_ctx, handle, val);
#endif
}

void start([[maybe_unused]] LCT_pcounter_handle_t handle)
{
#ifdef LCW_ENABLE_PCOUNTER
  LCT_pcounter_start(pcounter_ctx, handle);
#endif
}

void end([[maybe_unused]] LCT_pcounter_handle_t handle)
{
#ifdef LCW_ENABLE_PCOUNTER
  LCT_pcounter_end(pcounter_ctx, handle);
#endif
}
}  // namespace pcounter
}  // namespace lcw