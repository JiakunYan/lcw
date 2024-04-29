#ifndef LCW_SPINLOCK_HPP
#define LCW_SPINLOCK_HPP

namespace lcw
{
extern custom_spinlock_op_t spinlock_op;

void custom_spinlock_init();

class spinlock_t
{
 public:
  spinlock_t() { l = spinlock_op.alloc(); }

  ~spinlock_t() { spinlock_op.free(l); }

  bool try_lock() { return spinlock_op.trylock(l); }

  void lock() { spinlock_op.lock(l); }

  void unlock() { spinlock_op.unlock(l); }

 private:
  void* l;
};
}  // namespace lcw

#endif  // LCW_SPINLOCK_HPP
