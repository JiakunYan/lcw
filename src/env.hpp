#ifndef LCW_ENV_HPP
#define LCW_ENV_HPP

namespace lcw
{
struct config_t {
  bool force_default_spinlock = false;
};
extern config_t config;
}  // namespace lcw

#endif  // LCW_ENV_HPP
