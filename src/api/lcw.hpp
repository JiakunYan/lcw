#ifndef LCW_LCW_HPP
#define LCW_LCW_HPP

/**
 * @defgroup LCW_API Lightweight Communication Wrapper (LCW) API
 * @brief This section describes LCW API.
 */

#include <cstdint>
#include <string>
#include "lcw_config.hpp"
#define LCW_API __attribute__((visibility("default")))

namespace lcw
{
/**
 * @defgroup LCWX_ARGS LCWX Extendable arguments
 * @ingroup LCW_API
 * @brief Allow arbitrary arguments being passed in/out for future extension.
 */

/**
 * @defgroup LCW_SETUP LCW environment setup
 * @ingroup LCW_API
 * @brief Setup the communication resources and configurations.
 *
 * In order to use LCW, users need to first setup a few communication resources
 * and configurations, including devices and endpoints.
 */

/**
 * @defgroup LCW_DEVICE LCW device
 * @ingroup LCW_SETUP
 * @brief A device is a physical or logical resource that can be used for
 * communication. Communications that use the same device share a resource and
 * may affect each other's performance.
 */

/**
 * @defgroup LCW_COMM LCW communication
 * @ingroup LCW_API
 * @brief Communication-related calls.
 */

/**
 * @defgroup LCW_COMP LCW completion mechanism
 * @ingroup LCW_API
 */

/**
 * @ingroup LCW_DEVICE
 * @brief enum class for backend
 */
enum class backend_t {
  AUTO,
  LCI,
  MPI,
  LCI2,
  GEX,
};

struct device_opaque_t;
/**
 * @ingroup LCW_DEVICE
 * @brief The device type.
 */
using device_t = device_opaque_t*;

/**
 * @ingroup LCW_COMM
 * @brief The rank (process ID) type
 */
using rank_t = int64_t;

/**
 * @ingroup LCW_COMM
 * @brief The tag type
 */
using tag_t = int64_t;

struct comp_opaque_t;
/**
 * @ingroup LCW_COMM
 * @brief The general completion type
 */
using comp_t = comp_opaque_t*;

/**
 * @ingroup LCW_COMM
 * @brief operation code
 */
enum class op_t { SEND, RECV, PUT, PUT_SIGNAL };

/**
 * @ingroup LCW_COMP
 * @brief The request object
 */
struct request_t {
  op_t op;
  device_t device;
  rank_t rank;
  tag_t tag;
  void* buffer;
  int64_t length;
  void* user_context;
};

/**
 * @ingroup LCW_COMP
 * @brief The completion handler type
 */
using handler_t = void (*)(request_t request);

/**
 * @ingroup LCW_SETUP
 * @brief Initialize the LCW runtime. No LCW calls are allowed to be called
 * before LCW_initialize except @ref LCW_initialized.
 */
LCW_API void initialize(backend_t backend = backend_t::AUTO);

/**
 * @ingroup LCW_SETUP
 * @brief Check whether the LCW runtime has been initialized.
 */
LCW_API bool is_initialized();

/**
 * @ingroup LCW_SETUP
 * @brief Finalize the LCW runtime. No LCW calls are allowed to be called
 * after LCW_finalize except @ref LCW_initialized.
 */
LCW_API void finalize();

/**
 * @ingroup LCW_SETUP
 * @return The rank of the current process. Deprecated, use get_rank_me()
 * instead.
 */
LCW_API int64_t get_rank();

/**
 * @ingroup LCW_SETUP
 * @return The rank of the current process.
 */
LCW_API inline int64_t get_rank_me() { return get_rank(); }

/**
 * @ingroup LCW_SETUP
 * @return The total number of processes in this application. Deprecated, use
 * get_nranks_me() instead.
 */
LCW_API int64_t get_nranks();

/**
 * @ingroup LCW_SETUP
 * @return The total number of processes in this application.
 */
LCW_API inline int64_t get_rank_n() { return get_nranks(); }

/**
 * @ingroup LCW_DEVICE
 * @brief Allocate a device.
 * @return The device allocated.
 */
LCW_API device_t alloc_device(int64_t max_put_length = 0,
                              comp_t put_comp = nullptr);

/**
 * @ingroup LCW_DEVICE
 * @brief Free a device.
 * @param device the device to be freed.
 */
LCW_API void free_device(device_t device);

/**
 * @ingroup LCW_COMM
 * @param device the device to make progress on
 * @return Whether some works have been done
 */
LCW_API bool do_progress(device_t device);

/**
 * @ingroup LCW_COMM
 * @brief Allocate a completion handler
 */
LCW_API comp_t alloc_handler(handler_t handler);

/**
 * @ingroup LCW_COMM
 * @brief Free a completion handler
 */
LCW_API void free_handler(comp_t handler);

/**
 * @ingroup LCW_COMP
 * @brief Allocate a completion queue.
 */
LCW_API comp_t alloc_cq();

/**
 * @ingroup LCW_COMP
 * @brief Free a completion queue.
 */
LCW_API void free_cq(comp_t completion);

/**
 * @ingroup LCW_COMP
 * @brief poll a completion queue.
 */
LCW_API bool poll_cq(comp_t completion, request_t* request);

/**
 * @ingroup LCW_COMM
 * @brief post a send.
 * @return whether the send is posted successfully (true) or needs to be
 * retried (false).
 */
LCW_API bool send(device_t device, rank_t rank, tag_t tag, void* buf,
                  int64_t length, comp_t completion, void* user_context);

/**
 * @ingroup LCW_COMM
 * @brief post a receive.
 * @return whether the receive is posted successfully (true) or needs to be
 * retried (false).
 */
LCW_API bool recv(device_t device, rank_t rank, tag_t tag, void* buf,
                  int64_t length, comp_t completion, void* user_context);

/**
 * @ingroup LCW_COMM
 * @brief post a put
 * @return whether the put is posted successfully (true) or needs to be
 * retried (false).
 */
LCW_API bool put(device_t device, rank_t rank, void* buf, int64_t length,
                 comp_t completion, void* user_context);

/**
 * @ingroup LCW_COMM
 * @brief get the maximum tag that can be used for send/recv
 */
LCW_API tag_t get_max_tag(device_t device);

/**
 * @ingroup LCW_COMM
 * @brief barrier for all ranks
 */
LCW_API void barrier(device_t device);

/**
 * @ingroup LCW_SETUP
 * @brief Custom spinlock operations
 */
struct custom_spinlock_op_t {
  std::string name = "Unknown";
  void* (*alloc)() = nullptr;
  void (*free)(void*) = nullptr;
  void (*lock)(void*) = nullptr;
  bool (*trylock)(void*) = nullptr;
  void (*unlock)(void*) = nullptr;
};

/**
 * @ingroup LCW_SETUP
 * @brief Setup custom spinlock operations
 */
LCW_API void custom_spinlock_setup(custom_spinlock_op_t op);

/**
 * @ingroup LCW_SETUP
 * @brief Base class for custom memory allocator
 */
struct allocator_base_t {
  virtual void* allocate(size_t size) = 0;
  virtual void deallocate(void* ptr) = 0;
  virtual ~allocator_base_t() = default;
};

/**
 * @ingroup LCW_SETUP
 * @brief Setup a custom global default memory allocator
 */
LCW_API void set_custom_allocator(allocator_base_t* allocator);
}  // namespace lcw

#endif  // LCW_LCW_HPP
