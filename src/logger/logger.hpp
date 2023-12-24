#ifndef LCW_LOGGER_HPP
#define LCW_LOGGER_HPP

enum LCW_log_level_t {
  LCW_LOG_ERROR,
  LCW_LOG_WARN,
  LCW_LOG_DIAG,
  LCW_LOG_INFO,
  LCW_LOG_DEBUG,
  LCW_LOG_TRACE,
  LCW_LOG_MAX
};

extern LCT_log_ctx_t LCWI_log_ctx;

void LCWI_log_init();
void LCWI_log_fina();
static inline void LCW_Log_flush() { LCT_Log_flush(LCWI_log_ctx); }

#define LCW_Assert(...) LCT_Assert(LCWI_log_ctx, __VA_ARGS__)
#define LCW_Log(...) LCT_Log(LCWI_log_ctx, __VA_ARGS__)
#define LCW_Warn(...) LCW_Log(LCW_LOG_WARN, "warn", __VA_ARGS__)

#ifdef LCW_DEBUG
#define LCW_DBG_Assert(...) LCW_Assert(__VA_ARGS__)
#define LCW_DBG_Log(...) LCW_Log(__VA_ARGS__)
#define LCW_DBG_Warn(...) LCW_Warn(__VA_ARGS__)
#else
#define LCW_DBG_Assert(...)
#define LCW_DBG_Log(...)
#define LCW_DBG_Warn(...)
#endif

#endif  // LCW_LOGGER_HPP
