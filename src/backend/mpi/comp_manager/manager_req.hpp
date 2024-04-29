#ifndef LCW_MANAGER_REQ_HPP
#define LCW_MANAGER_REQ_HPP

#include <mpi.h>
#include "lcwi.hpp"
#include "manager_base.hpp"

namespace lcw
{
namespace mpi
{
namespace comp
{
struct manager_req_t : public manager_base_t {
  manager_req_t() = default;
  ~manager_req_t() override = default;
  void add_entry(entry_t entry) override;
  bool do_progress() override;

  std::deque<entry_t> entries;
  spinlock_t lock;
};
}  // namespace comp
}  // namespace mpi
}  // namespace lcw

#endif  // LCW_MANAGER_REQ_HPP
