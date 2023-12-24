#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <cassert>
#include <vector>
#include "lcw.hpp"
#include "comm_exp.hpp"

/**
 * Multithreaded ping-pong benchmark
 */

void worker_thread_fn(int thread_id);
void progress_thread_fn(int thread_id);

struct Config {
  int nthreads = 1;
  int min_size = 8;
  int max_size = 8192;
  int niters = 1000;
  bool touch_data = false;
  bool pin_thread = true;
  int nprgthreads = 0;
};

const size_t NPROCESSORS = sysconf(_SC_NPROCESSORS_ONLN);

lcw::device_t device;
Config config;
static std::atomic<bool> progress_thread_stop(false);

void worker_thread_fn(int thread_id)
{
  fprintf(stderr, "I am worker thread %d\n", thread_id);
  int64_t rank = lcw::get_rank();
  int64_t nranks = lcw::get_nranks();
  int64_t peer_rank = (rank + nranks / 2) % nranks;
}

void progress_thread_fn(int thread_id)
{
  fprintf(stderr, "I am progress thread %d\n", thread_id);
}

int main(int argc, char* argv[])
{
  if (argc > 1) config.nthreads = atoi(argv[1]);
  if (argc > 2) config.min_size = atoi(argv[3]);
  if (argc > 3) config.max_size = atoi(argv[4]);
  if (argc > 4) config.touch_data = atoi(argv[5]);

  lcw::initialize(lcw::backend_t::MPI);
  device = lcw::alloc_device();

  assert(config.nthreads > config.nprgthreads);
  assert(config.nthreads > 0);
  assert(config.nprgthreads >= 0);
  if (config.nthreads > NPROCESSORS) {
    fprintf(stderr, "WARNING: The total thread number (%d) is larger than the total processor number (%lu).\n",
            config.nthreads, NPROCESSORS);
  }
  if (config.nthreads == 1) {
    worker_thread_fn(0);
  } else {
    std::vector<std::thread> worker_pool;
    std::vector<std::thread> progress_pool;
    if (config.nprgthreads != 0)
      int nthreads_per_prg = config.nthreads / config.nprgthreads;
    for (int i = 0; i < config.nthreads; ++i) {
      if (config.nprgthreads > 0 && i % config.nprgthreads == 0) {
        // spawn a progress thread
        std::thread t(progress_thread_fn, progress_pool.size());
        if (config.pin_thread)
          set_affinity(t.native_handle(), i % NPROCESSORS);
        progress_pool.push_back(std::move(t));
      } else {
        // spawn a worker thread
        // spawn a progress thread
        std::thread t(worker_thread_fn, worker_pool.size());
        if (config.pin_thread)
          set_affinity(t.native_handle(), i % NPROCESSORS);
        worker_pool.push_back(std::move(t));
      }
    }

    for (auto &t : worker_pool) {
      t.join();
    }
    progress_thread_stop = true;
    for (auto &t : progress_pool) {
      t.join();
    }
  }
  lcw::finalize();
  return EXIT_SUCCESS;
}