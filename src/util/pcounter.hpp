#ifndef LCW_PCOUNTER_HPP
#define LCW_PCOUNTER_HPP

namespace lcw
{
namespace pcounter
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

#define LCW_PCOUNTER_TIMER_FOR_EACH(_macro) \
    _macro(put_timer)                       \
    _macro(send_timer)                      \
    _macro(recv_timer)
// clang-format on

extern LCT_pcounter_ctx_t pcounter_ctx;
#define LCW_PCOUNTER_HANDLE_DECL(name) extern LCT_pcounter_handle_t name;

LCW_PCOUNTER_NONE_FOR_EACH(LCW_PCOUNTER_HANDLE_DECL)
LCW_PCOUNTER_TREND_FOR_EACH(LCW_PCOUNTER_HANDLE_DECL)
LCW_PCOUNTER_TIMER_FOR_EACH(LCW_PCOUNTER_HANDLE_DECL)

void initialize();
void finalize();
int64_t now();
int64_t since(int64_t then);
void add(LCT_pcounter_handle_t handle, int64_t val = 1);
void start(LCT_pcounter_handle_t handle);
void end(LCT_pcounter_handle_t handle);
}  // namespace pcounter
}  // namespace lcw

#endif  // LCW_PCOUNTER_HPP
