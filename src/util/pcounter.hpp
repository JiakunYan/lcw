#ifndef LCW_PCOUNTER_HPP
#define LCW_PCOUNTER_HPP

namespace lcw
{
// clang-format off
#define LCW_PCOUNTER_NONE_FOR_EACH(_macro)

#define LCW_PCOUNTER_TREND_FOR_EACH(_macro) \
    _macro(put_start)                       \
    _macro(put_end)                         \
    _macro(put_retry)                       \
    _macro(send_start)                      \
    _macro(send_end)                        \
    _macro(send_retry)                      \
    _macro(recv_start)                      \
    _macro(recv_end)                        \
    _macro(recv_retry)                      \
    _macro(put_signal)                      \
    _macro(comp_produce)                    \
    _macro(comp_consume)                    \
    _macro(comp_poll)                       \
    _macro(progress)

#define LCW_PCOUNTER_TIMER_FOR_EACH(_macro)
// clang-format on

extern LCT_pcounter_ctx_t pcounter_ctx;
#define LCW_PCOUNTER_HANDLE_DECL(name) extern LCT_pcounter_handle_t name;

LCW_PCOUNTER_NONE_FOR_EACH(LCW_PCOUNTER_HANDLE_DECL)
LCW_PCOUNTER_TREND_FOR_EACH(LCW_PCOUNTER_HANDLE_DECL)
LCW_PCOUNTER_TIMER_FOR_EACH(LCW_PCOUNTER_HANDLE_DECL)

void init_pcounter();
void free_pcounter();
int64_t pcounter_now();
int64_t pcounter_since(int64_t then);
void pcounter_add(LCT_pcounter_handle_t handle, int64_t val = 1);
void pcounter_start(LCT_pcounter_handle_t handle);
void pcounter_end(LCT_pcounter_handle_t handle);
}  // namespace lcw

#endif  // LCW_PCOUNTER_HPP
