#include "lcwi.hpp"

LCT_log_ctx_t LCWI_log_ctx;

void LCWI_log_init()
{
  const char* const log_levels[] = {
      [LCW_LOG_ERROR] = "error", [LCW_LOG_WARN] = "warn",
      [LCW_LOG_DIAG] = "diag",   [LCW_LOG_INFO] = "info",
      [LCW_LOG_DEBUG] = "debug", [LCW_LOG_TRACE] = "trace"};
  LCWI_log_ctx = LCT_log_ctx_alloc(
      log_levels, sizeof(log_levels) / sizeof(log_levels[0]), LCW_LOG_WARN,
      "LCW", getenv("LCW_LOG_OUTFILE"), getenv("LCW_LOG_LEVEL"),
      getenv("LCW_LOG_WHITELIST"), getenv("LCW_LOG_BLACKLIST"));
}

void LCWI_log_fina() { LCT_log_ctx_free(&LCWI_log_ctx); }