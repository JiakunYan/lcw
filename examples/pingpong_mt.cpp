#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <cassert>
#include <vector>
#include <cstring>
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
  int niters = 10;
  bool test_mode = false;
  bool pin_thread = true;
  int nprgthreads = 0;
};

const size_t NPROCESSORS = sysconf(_SC_NPROCESSORS_ONLN);
const size_t PAGESIZE = sysconf(_SC_PAGESIZE);

lcw::device_t device;
Config config;
static std::atomic<bool> progress_thread_stop(false);

void worker_thread_fn(int thread_id)
{
  int64_t rank = lcw::get_rank();
  int64_t nranks = lcw::get_nranks();
  int64_t peer_rank = (rank + nranks / 2) % nranks;
  bool need_progress = config.nprgthreads == 0;
  int nworkers = config.nthreads - config.nprgthreads;
  int tag = thread_id;
  lcw::comp_t cq = lcw::alloc_cq();
  for (int msg_size = config.min_size; msg_size <= config.max_size;
       msg_size *= 2) {
    tag += nworkers;  // every message size uses different tag
    void *send_buffer, *recv_buffer;
    int ret;
    ret = posix_memalign(&send_buffer, PAGESIZE, msg_size);
    assert(ret == 0);
    ret = posix_memalign(&recv_buffer, PAGESIZE, msg_size);
    assert(ret == 0);
    for (int i = 0; i < config.niters; ++i) {
      char seed = 'a' + thread_id + i;
      if (config.test_mode) {
        write_buffer((char*)send_buffer, msg_size, seed);
        memset(recv_buffer, 0, msg_size);
      }
      if (nranks == 1 || rank < nranks / 2) {
        // The sender
        // First post a recv
        lcw::request_t rreq;
        while (!lcw::recv(device, peer_rank, tag, recv_buffer, msg_size, cq,
                          reinterpret_cast<void*>(i))) {
          if (need_progress) lcw::do_progress(device);
        }
        // Then post a send
        while (!lcw::send(device, peer_rank, tag, send_buffer, msg_size, cq,
                          reinterpret_cast<void*>(i))) {
          if (need_progress) lcw::do_progress(device);
        }
        // Wait for the send to complete
        lcw::request_t sreq;
        while (!lcw::poll_cq(cq, &sreq)) {
          if (need_progress) lcw::do_progress(device);
        }
        if (config.test_mode) {
          assert(sreq.device == device);
          assert(sreq.length == msg_size);
          assert(sreq.user_context == reinterpret_cast<void*>(i));
          assert(sreq.op == lcw::op_t::SEND);
          assert(sreq.buffer == send_buffer);
          assert(sreq.rank == peer_rank);
          assert(sreq.tag == tag);
        }
        // Wait for the recv to complete
        while (!lcw::poll_cq(cq, &rreq)) {
          if (need_progress) lcw::do_progress(device);
        }
        if (config.test_mode) {
          assert(rreq.device == device);
          assert(rreq.length == msg_size);
          assert(rreq.user_context == reinterpret_cast<void*>(i));
          assert(rreq.op == lcw::op_t::RECV);
          assert(rreq.buffer == send_buffer);
          assert(rreq.rank == peer_rank);
          assert(rreq.tag == tag);
          check_buffer((char*)recv_buffer, msg_size, seed);
        }
      } else {
        // The receiver
        // first recv
        lcw::request_t rreq;
        while (!lcw::recv(device, peer_rank, tag, recv_buffer, msg_size, cq,
                          reinterpret_cast<void*>(i))) {
          if (need_progress) lcw::do_progress(device);
        }
        while (!lcw::poll_cq(cq, &rreq)) {
          if (need_progress) lcw::do_progress(device);
        }
        if (config.test_mode) {
          assert(rreq.device == device);
          assert(rreq.length == msg_size);
          assert(rreq.user_context == reinterpret_cast<void*>(i));
          assert(rreq.op == lcw::op_t::RECV);
          assert(rreq.buffer == send_buffer);
          assert(rreq.rank == peer_rank);
          assert(rreq.tag == tag);
          check_buffer((char*)recv_buffer, msg_size, seed);
        }
        // then send
        while (!lcw::send(device, peer_rank, tag, send_buffer, msg_size, cq,
                          reinterpret_cast<void*>(i))) {
          if (need_progress) lcw::do_progress(device);
        }
        lcw::request_t sreq;
        while (!lcw::poll_cq(cq, &sreq)) {
          if (need_progress) lcw::do_progress(device);
        }
        if (config.test_mode) {
          assert(sreq.device == device);
          assert(sreq.length == msg_size);
          assert(sreq.user_context == reinterpret_cast<void*>(i));
          assert(sreq.op == lcw::op_t::SEND);
          assert(sreq.buffer == send_buffer);
          assert(sreq.rank == peer_rank);
          assert(sreq.tag == tag);
        }
      }
    }
  }
}

void progress_thread_fn(int thread_id)
{
  fprintf(stderr, "I am progress thread %d\n", thread_id);
  while (progress_thread_stop) {
    lcw::do_progress(device);
  }
}

int main(int argc, char* argv[])
{
  if (argc > 1) config.nthreads = atoi(argv[1]);
  if (argc > 2) config.min_size = atoi(argv[3]);
  if (argc > 3) config.max_size = atoi(argv[4]);
  if (argc > 4) config.test_mode = atoi(argv[5]);

  lcw::initialize(lcw::backend_t::MPI);
  device = lcw::alloc_device();

  assert(config.nthreads > config.nprgthreads);
  assert(config.nthreads > 0);
  assert(config.nprgthreads >= 0);
  if (config.nthreads > NPROCESSORS) {
    fprintf(stderr,
            "WARNING: The total thread number (%d) is larger than the total "
            "processor number (%lu).\n",
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
        if (config.pin_thread) set_affinity(t.native_handle(), i % NPROCESSORS);
        progress_pool.push_back(std::move(t));
      } else {
        // spawn a worker thread
        // spawn a progress thread
        std::thread t(worker_thread_fn, worker_pool.size());
        if (config.pin_thread) set_affinity(t.native_handle(), i % NPROCESSORS);
        worker_pool.push_back(std::move(t));
      }
    }

    for (auto& t : worker_pool) {
      t.join();
    }
    progress_thread_stop = true;
    for (auto& t : progress_pool) {
      t.join();
    }
  }
  lcw::finalize();
  return EXIT_SUCCESS;
}