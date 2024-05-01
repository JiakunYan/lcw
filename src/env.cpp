#include "lcwi.hpp"

namespace lcw
{
config_t config;

void init_env()
{
  // Default completion queue length
  {
    char* p = getenv("LCW_FORCE_PTHREAD_SPINLOCK");
    if (p) {
      config.force_default_spinlock = atoi(p);
    }
    LCW_Log(LCW_LOG_INFO, "env", "Set LCW_FORCE_PTHREAD_SPINLOCK to %d\n",
            config.force_default_spinlock);
  }
}
}  // namespace lcw