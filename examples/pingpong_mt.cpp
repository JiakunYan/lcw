#include <iostream>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <cassert>
#include <vector>
#include <cstring>
#include <getopt.h>
#include "lcw.hpp"
#include "lct.h"
#include "comm_exp.hpp"

/**
 * Multithreaded ping-pong benchmark
 */

void worker_thread_fn(int thread_id);
void progress_thread_fn(int thread_id);

struct Config {
  lcw::op_t op = lcw::op_t::SEND;
  int nthreads = 1;
  int min_size = 8;
  int max_size = 8192;
  int niters = 10;
  int test_mode = 0;
  int pin_thread = 1;
  int nprgthreads = 0;
};

const size_t NPROCESSORS = sysconf(_SC_NPROCESSORS_ONLN);
const size_t PAGESIZE = sysconf(_SC_PAGESIZE);

lcw::device_t device;
lcw::comp_t cq;
Config config;
static std::atomic<bool> progress_thread_stop(false);
LCT_tbarrier_t tbarrier_all;
LCT_tbarrier_t tbarrier_worker;

void worker_thread_fn(int thread_id)
{
  LCT_tbarrier_arrive_and_wait(tbarrier_all);
  int64_t rank = lcw::get_rank();
  int64_t nranks = lcw::get_nranks();
  int64_t peer_rank = (rank + nranks / 2) % nranks;
  bool need_progress = config.nprgthreads == 0;
  int nworkers = config.nthreads - config.nprgthreads;
  int tag = thread_id;
  for (int msg_size = config.min_size; msg_size <= config.max_size;
       msg_size *= 2) {
    tag += nworkers;  // every message size uses different tag
    void *send_buffer, *recv_buffer;
    int ret;
    ret = posix_memalign(&send_buffer, PAGESIZE, msg_size);
    assert(ret == 0);
    ret = posix_memalign(&recv_buffer, PAGESIZE, msg_size);
    assert(ret == 0);
    LCT_tbarrier_arrive_and_wait(tbarrier_worker);
    auto start_time = LCT_now();
    for (int i = 0; i < config.niters; ++i) {
      unsigned int seed = thread_id + i;
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
    LCT_tbarrier_arrive_and_wait(tbarrier_worker);
    auto total_time = LCT_now() - start_time;
    double total_time_s = LCT_time_to_s(total_time);
    double msg_rate = config.niters * nworkers / total_time_s;
    double bandwidth = msg_rate * msg_size;
    if (rank == 0) {
      std::cout << "====================================\n"
                << "Message size (B): " << msg_size << "\n"
                << "Total time (s): " << total_time_s << "\n"
                << "Latency (us): " << total_time_s * 1e6 / config.niters
                << "\n"
                << "Message Rate (K/s): " << msg_rate / 1e3 << "\n"
                << "Bandwidth (MB/s): " << bandwidth / 1e6 << std::endl;
    }
  }
}

void progress_thread_fn(int thread_id)
{
  LCT_tbarrier_arrive_and_wait(tbarrier_all);
  while (progress_thread_stop) {
    lcw::do_progress(device);
  }
}

int main(int argc, char* argv[])
{
  lcw::initialize(lcw::backend_t::MPI);

  LCT_args_parser_t argsParser = LCT_args_parser_alloc();
  LCT_dict_str_int_t dict[] = {{"send", (int)lcw::op_t::SEND},
                               {"put", (int)lcw::op_t::PUT}};
  LCT_args_parser_add_dict(argsParser, "op", required_argument,
                           (int*)&config.op, dict, 2);
  LCT_args_parser_add(argsParser, "nthreads", required_argument,
                      &config.nthreads);
  LCT_args_parser_add(argsParser, "min-size", required_argument,
                      &config.min_size);
  LCT_args_parser_add(argsParser, "max-size", required_argument,
                      &config.max_size);
  LCT_args_parser_add(argsParser, "niters", required_argument, &config.niters);
  LCT_args_parser_add(argsParser, "test-mode", required_argument,
                      &config.test_mode);
  LCT_args_parser_add(argsParser, "pin-thread", required_argument,
                      &config.pin_thread);
  LCT_args_parser_add(argsParser, "nprgthreads", required_argument,
                      &config.nprgthreads);
  LCT_args_parser_parse(argsParser, argc, argv);
  if (lcw::get_rank() == 0) LCT_args_parser_print(argsParser, true);
  LCT_args_parser_free(argsParser);

  cq = lcw::alloc_cq();
  if (config.op == lcw::op_t::SEND)
    device = lcw::alloc_device();
  else
    device = lcw::alloc_device(config.max_size, cq);

  assert(config.nthreads > config.nprgthreads);
  assert(config.nthreads > 0);
  assert(config.nprgthreads >= 0);
  if (config.nthreads > NPROCESSORS) {
    fprintf(stderr,
            "WARNING: The total thread number (%d) is larger than the total "
            "processor number (%lu).\n",
            config.nthreads, NPROCESSORS);
  }
  tbarrier_all = LCT_tbarrier_alloc(config.nthreads);
  tbarrier_worker = LCT_tbarrier_alloc(config.nthreads - config.nprgthreads);
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
  lcw::free_device(device);
  lcw::finalize();
  return EXIT_SUCCESS;
}