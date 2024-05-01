#include "lcwi.hpp"

namespace lcw
{
namespace detail
{
void* spinlock_pthread_alloc()
{
  pthread_spinlock_t* l = new pthread_spinlock_t;
  pthread_spin_init(l, PTHREAD_PROCESS_PRIVATE);
  return (void*)l;
}

void spinlock_pthread_free(void* p)
{
  auto l = reinterpret_cast<pthread_spinlock_t*>(p);
  pthread_spin_destroy(l);
  delete l;
}

void spinlock_pthread_lock(void* p)
{
  auto l = reinterpret_cast<pthread_spinlock_t*>(p);
  pthread_spin_lock(l);
}

bool spinlock_pthread_trylock(void* p)
{
  auto l = reinterpret_cast<pthread_spinlock_t*>(p);
  return pthread_spin_trylock(l) == 0;
}

void spinlock_pthread_unlock(void* p)
{
  auto l = reinterpret_cast<pthread_spinlock_t*>(p);
  pthread_spin_unlock(l);
}
}  // namespace detail

bool setuped = false;
custom_spinlock_op_t spinlock_op;
void custom_spinlock_setup(custom_spinlock_op_t op)
{
  if (setuped) {
    LCW_Warn(
        "Custom spinlock has already been set up as %s. "
        "The program may not work as expected.\n",
        spinlock_op.name.c_str());
  }
  setuped = true;
  spinlock_op = op;
}

void custom_spinlock_init()
{
  if (!setuped || config.force_default_spinlock) {
    spinlock_op.name = "pthread";
    spinlock_op.alloc = detail::spinlock_pthread_alloc;
    spinlock_op.free = detail::spinlock_pthread_free;
    spinlock_op.lock = detail::spinlock_pthread_lock;
    spinlock_op.trylock = detail::spinlock_pthread_trylock;
    spinlock_op.unlock = detail::spinlock_pthread_unlock;
  }
  LCW_Log(LCW_LOG_INFO, "init", "Use custom spinlock: %s\n",
          spinlock_op.name.c_str());
}
}  // namespace lcw