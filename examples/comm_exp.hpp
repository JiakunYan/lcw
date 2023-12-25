#ifndef COMM_EXP_H_
#define COMM_EXP_H_

#include <sys/time.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>

void set_affinity(pthread_t pthread_handler, size_t target)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(target, &cpuset);
  int rv = pthread_setaffinity_np(pthread_handler, sizeof(cpuset), &cpuset);
  if (rv != 0) {
    fprintf(stderr, "ERROR %d thread affinity didn't work.\n", rv);
    exit(1);
  }
}

void write_buffer(char* buf, size_t size, unsigned int seed)
{
  for (int i = 0; i < size; ++i) {
    buf[i] = static_cast<char>(rand_r(&seed));
  }
}

bool check_buffer(const char* buf, size_t size, unsigned int seed)
{
  for (int i = 0; i < size; ++i) {
    if (buf[i] != static_cast<char>(rand_r(&seed))) {
      return false;
    }
  }
  return true;
}

static inline double wtime()
{
  struct timeval t1;
  gettimeofday(&t1, 0);
  return t1.tv_sec + t1.tv_usec / 1e6;
}

static inline double wutime()
{
  struct timeval t1;
  gettimeofday(&t1, 0);
  return t1.tv_sec * 1e6 + t1.tv_usec;
}

#define max(a, b) ((a > b) ? (a) : (b))

static inline unsigned long long get_rdtsc()
{
  unsigned hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  unsigned long long cycle =
      ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
  return cycle;
}

static inline void busywait_cyc(unsigned long long delay)
{
  unsigned long long start_cycle, stop_cycle, start_plus_delay;
  start_cycle = get_rdtsc();
  start_plus_delay = start_cycle + delay;
  do {
    stop_cycle = get_rdtsc();
  } while (stop_cycle < start_plus_delay);
}

#ifdef __cplusplus
#include <mutex>
#include <condition_variable>
class cpp_barrier
{
 private:
  std::mutex _mutex;
  std::condition_variable _cv;
  std::size_t _count;

 public:
  explicit cpp_barrier(std::size_t count) : _count{count} {}
  void wait()
  {
    std::unique_lock<std::mutex> lock{_mutex};
    if (--_count == 0) {
      _cv.notify_all();
    } else {
      _cv.wait(lock, [this] { return _count == 0; });
    }
  }
};
#endif
#endif
