#include "lcwi.hpp"

namespace lcw
{
namespace detail
{
#ifndef __APPLE__
void* spinlock_pthread_spinlock_alloc()
{
  pthread_spinlock_t* l = new pthread_spinlock_t;
  pthread_spin_init(l, PTHREAD_PROCESS_PRIVATE);
  return (void*)l;
}

void spinlock_pthread_spinlock_free(void* p)
{
  auto l = reinterpret_cast<pthread_spinlock_t*>(p);
  pthread_spin_destroy(l);
  delete l;
}

void spinlock_pthread_spinlock_lock(void* p)
{
  auto l = reinterpret_cast<pthread_spinlock_t*>(p);
  pthread_spin_lock(l);
}

bool spinlock_pthread_spinlock_trylock(void* p)
{
  auto l = reinterpret_cast<pthread_spinlock_t*>(p);
  return pthread_spin_trylock(l) == 0;
}

void spinlock_pthread_spinlock_unlock(void* p)
{
  auto l = reinterpret_cast<pthread_spinlock_t*>(p);
  pthread_spin_unlock(l);
}
#endif  // ndef __APPLE__

void* spinlock_pthread_mutex_alloc()
{
  pthread_mutex_t* l = new pthread_mutex_t;
  pthread_mutex_init(l, nullptr);
  return (void*)l;
}

void spinlock_pthread_mutex_free(void* p)
{
  auto l = reinterpret_cast<pthread_mutex_t*>(p);
  pthread_mutex_destroy(l);
  delete l;
}

void spinlock_pthread_mutex_lock(void* p)
{
  auto l = reinterpret_cast<pthread_mutex_t*>(p);
  pthread_mutex_lock(l);
}

bool spinlock_pthread_mutex_trylock(void* p)
{
  auto l = reinterpret_cast<pthread_mutex_t*>(p);
  return pthread_mutex_trylock(l) == 0;
}

void spinlock_pthread_mutex_unlock(void* p)
{
  auto l = reinterpret_cast<pthread_mutex_t*>(p);
  pthread_mutex_unlock(l);
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
#ifdef __APPLE__
    spinlock_op.name = "pthread_mutex";
    spinlock_op.alloc = detail::spinlock_pthread_mutex_alloc;
    spinlock_op.free = detail::spinlock_pthread_mutex_free;
    spinlock_op.lock = detail::spinlock_pthread_mutex_lock;
    spinlock_op.trylock = detail::spinlock_pthread_mutex_trylock;
    spinlock_op.unlock = detail::spinlock_pthread_mutex_unlock;
#else
    spinlock_op.name = "pthread_spinlock";
    spinlock_op.alloc = detail::spinlock_pthread_spinlock_alloc;
    spinlock_op.free = detail::spinlock_pthread_spinlock_free;
    spinlock_op.lock = detail::spinlock_pthread_spinlock_lock;
    spinlock_op.trylock = detail::spinlock_pthread_spinlock_trylock;
    spinlock_op.unlock = detail::spinlock_pthread_spinlock_unlock;
#endif
  }
  LCW_Log(LCW_LOG_INFO, "init", "Use custom spinlock: %s\n",
          spinlock_op.name.c_str());
}
}  // namespace lcw