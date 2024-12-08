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

void worker_thread_fn(int worker_id);
void progress_thread_fn(int progress_id);

struct Config {
  lcw::op_t op = lcw::op_t::SEND;
  int ndevices = 2;
  int nthreads = 4;
  int min_size = 8;
  int max_size = 8192;
  int niters = 10;
  int test_mode = 1;
  int pin_thread = 1;
  int nprgthreads = 1;
  int send_window = 1;
  int recv_window = 1;
};

const size_t NPROCESSORS = sysconf(_SC_NPROCESSORS_ONLN);
const size_t PAGESIZE = sysconf(_SC_PAGESIZE);

struct device_t {
  lcw::device_t device;
  lcw::comp_t put_cq;
};
std::vector<device_t> devices;
Config config;
static std::atomic<bool> progress_thread_stop(false);
LCT_tbarrier_t tbarrier_all;
LCT_tbarrier_t tbarrier_worker;

int get_tag(int worker_id, int iter, int idx)
{
  int nworkers = config.nthreads - config.nprgthreads;
  return worker_id + iter * nworkers + idx * config.niters * nworkers;
}

device_t& get_device(int worker_id)
{
  assert(devices.size() == config.ndevices);
  int nworkers = config.nthreads - config.nprgthreads;
  int nworkers_per_device = (nworkers + devices.size() - 1) / devices.size();
  int device_idx = worker_id / nworkers_per_device;
  assert(devices.size() > device_idx);
  // We need to allocate the devices sequentially in the same order
  for (int i = 0; i < devices.size(); ++i) {
    if (worker_id % nworkers_per_device == 0 && device_idx == i) {
      // allocate the device
      if (config.op == lcw::op_t::SEND)
        devices[device_idx].device = lcw::alloc_device();
      else {
        devices[device_idx].put_cq = lcw::alloc_cq();
        devices[device_idx].device =
            lcw::alloc_device(config.max_size, devices[device_idx].put_cq);
      }
      LCT_tbarrier_arrive_and_wait(tbarrier_worker);
    } else {
      // wait for the device to be allocated
      LCT_tbarrier_arrive_and_wait(tbarrier_worker);
    }
  }
  // if (worker_id % nworkers_per_device == 0) {
  //   // allocate the device
  //   if (config.op == lcw::op_t::SEND)
  //     devices[device_idx].device = lcw::alloc_device();
  //   else {
  //     devices[device_idx].put_cq = lcw::alloc_cq();
  //     devices[device_idx].device = lcw::alloc_device(config.max_size,
  //     devices[device_idx].put_cq);
  //   }
  //   LCT_tbarrier_arrive_and_wait(tbarrier_worker);
  // } else {
  //   // wait for the device to be allocated
  //   LCT_tbarrier_arrive_and_wait(tbarrier_worker);
  // }
  return devices[device_idx];
}

void worker_thread_fn(int worker_id)
{
  device_t& device = get_device(worker_id);
  LCT_tbarrier_arrive_and_wait(tbarrier_all);
  int64_t rank = lcw::get_rank();
  int64_t nranks = lcw::get_nranks();
  int64_t peer_rank = (rank + nranks / 2) % nranks;
  bool need_progress = config.nprgthreads == 0;
  int nworkers = config.nthreads - config.nprgthreads;
  lcw::comp_t scq = lcw::alloc_cq();
  lcw::comp_t rcq;
  if (config.op == lcw::op_t::SEND)
    rcq = lcw::alloc_cq();
  else
    rcq = device.put_cq;
  for (int msg_size = config.min_size; msg_size <= config.max_size;
       msg_size *= 2) {
    char *send_buffer, *recv_buffer;
    int ret;
    ret = posix_memalign((void**)&send_buffer, PAGESIZE,
                         msg_size * config.send_window);
    assert(ret == 0);
    ret = posix_memalign((void**)&recv_buffer, PAGESIZE,
                         msg_size * config.recv_window);
    assert(ret == 0);
    LCT_tbarrier_arrive_and_wait(tbarrier_worker);
    auto start_time = LCT_now();
    for (int i = 0; i < config.niters; ++i) {
      if (nranks == 1 || rank < nranks / 2) {
        // The sender
        // First post "recv_window" recv
        if (config.op == lcw::op_t::SEND) {
          for (int j = 0; j < config.recv_window; ++j) {
            if (config.test_mode) {
              memset(recv_buffer + j * msg_size, 0, msg_size);
            }
            while (!lcw::recv(device.device, peer_rank,
                              get_tag(worker_id, i, j),
                              recv_buffer + j * msg_size, msg_size, rcq,
                              reinterpret_cast<void*>(i))) {
              if (need_progress) lcw::do_progress(device.device);
            }
          }
        }
        // Then post "send_window" send
        for (int j = 0; j < config.send_window; ++j) {
          // Prepare the data to send
          if (config.test_mode) {
            write_buffer((char*)send_buffer + j * msg_size, msg_size,
                         get_tag(worker_id, i, j));
          }
          while (true) {
            bool ret;
            if (config.op == lcw::op_t::SEND)
              ret =
                  lcw::send(device.device, peer_rank, get_tag(worker_id, i, j),
                            send_buffer + j * msg_size, msg_size, scq,
                            reinterpret_cast<void*>(i));
            else
              ret =
                  lcw::put(device.device, peer_rank, send_buffer + j * msg_size,
                           msg_size, scq, reinterpret_cast<void*>(i));
            if (ret) break;
            if (need_progress) lcw::do_progress(device.device);
          }
        }
        // Wait for all sends to complete
        for (int j = 0; j < config.send_window; ++j) {
          lcw::request_t sreq;
          while (!lcw::poll_cq(scq, &sreq)) {
            if (need_progress) lcw::do_progress(device.device);
          }
          if (config.test_mode) {
            assert(sreq.device == device.device);
            assert(sreq.length == msg_size);
            assert(sreq.user_context == reinterpret_cast<void*>(i));
            assert(sreq.op == config.op);
            assert(sreq.buffer == send_buffer + j * msg_size);
            assert(sreq.rank == peer_rank);
            if (config.op == lcw::op_t::SEND)
              assert(sreq.tag == get_tag(worker_id, i, j));
          }
        }
        // Wait for the recv to complete
        for (int j = 0; j < config.recv_window; ++j) {
          lcw::request_t rreq;
          while (!lcw::poll_cq(rcq, &rreq)) {
            if (need_progress) lcw::do_progress(device.device);
          }
          if (config.test_mode) {
            assert(rreq.device == device.device ||
                   rreq.op == lcw::op_t::PUT_SIGNAL);
            assert(rreq.length == msg_size);
            assert(rreq.op == ((config.op == lcw::op_t::SEND)
                                   ? lcw::op_t::RECV
                                   : lcw::op_t::PUT_SIGNAL));
            assert(rreq.rank == peer_rank);
            if (config.op == lcw::op_t::SEND) {
              assert(rreq.buffer == recv_buffer + j * msg_size);
              assert(rreq.user_context == reinterpret_cast<void*>(i));
              assert(rreq.tag == get_tag(worker_id, i, j));
              check_buffer((char*)recv_buffer, msg_size);
            } else {
              check_buffer((char*)rreq.buffer, msg_size);
              free(rreq.buffer);
            }
          }
        }
      } else {
        // The receiver
        // first recv
        for (int j = 0; j < config.send_window; ++j) {
          if (config.test_mode) {
            memset(recv_buffer + j * msg_size, 0, msg_size);
          }
          if (config.op == lcw::op_t::SEND) {
            while (!lcw::recv(device.device, peer_rank,
                              get_tag(worker_id, i, j),
                              recv_buffer + j * msg_size, msg_size, rcq,
                              reinterpret_cast<void*>(i))) {
              if (need_progress) lcw::do_progress(device.device);
            }
          }
        }
        for (int j = 0; j < config.send_window; ++j) {
          lcw::request_t rreq;
          while (!lcw::poll_cq(rcq, &rreq)) {
            if (need_progress) lcw::do_progress(device.device);
          }
          if (config.test_mode) {
            assert(rreq.device == device.device);
            assert(rreq.length == msg_size);
            assert(rreq.op == ((config.op == lcw::op_t::SEND)
                                   ? lcw::op_t::RECV
                                   : lcw::op_t::PUT_SIGNAL));
            assert(rreq.rank == peer_rank);
            if (config.op == lcw::op_t::SEND) {
              assert(rreq.buffer == recv_buffer + j * msg_size);
              assert(rreq.user_context == reinterpret_cast<void*>(i));
              assert(rreq.tag == get_tag(worker_id, i, j));
              check_buffer((char*)recv_buffer, msg_size);
            } else {
              check_buffer((char*)rreq.buffer, msg_size);
              free(rreq.buffer);
            }
          }
        }
        // then send
        for (int j = 0; j < config.recv_window; ++j) {
          // Prepare the data to send
          if (config.test_mode) {
            write_buffer((char*)send_buffer + j * msg_size, msg_size,
                         get_tag(worker_id, i, j));
          }
          while (true) {
            bool ret;
            if (config.op == lcw::op_t::SEND)
              ret =
                  lcw::send(device.device, peer_rank, get_tag(worker_id, i, j),
                            send_buffer + j * msg_size, msg_size, scq,
                            reinterpret_cast<void*>(i));
            else
              ret =
                  lcw::put(device.device, peer_rank, send_buffer + j * msg_size,
                           msg_size, scq, reinterpret_cast<void*>(i));
            if (ret) break;
            if (need_progress) lcw::do_progress(device.device);
          }
        }
        for (int j = 0; j < config.recv_window; ++j) {
          lcw::request_t sreq;
          while (!lcw::poll_cq(scq, &sreq)) {
            if (need_progress) lcw::do_progress(device.device);
          }
          if (config.test_mode) {
            assert(sreq.device == device.device);
            assert(sreq.length == msg_size);
            assert(sreq.user_context == reinterpret_cast<void*>(i));
            assert(sreq.op == config.op);
            assert(sreq.buffer == send_buffer);
            assert(sreq.rank == peer_rank);
            if (config.op == lcw::op_t::SEND)
              assert(sreq.tag == get_tag(worker_id, i, j));
          }
        }
      }
    }
    LCT_tbarrier_arrive_and_wait(tbarrier_worker);
    auto total_time = LCT_now() - start_time;
    double total_time_s = LCT_time_to_s(total_time);
    double msg_rate = config.niters * nworkers * nranks * config.send_window /
                      2 / total_time_s;
    double bandwidth = msg_rate * msg_size;
    if (rank == 0 && worker_id == 0) {
      std::cout << "====================================\n"
                << "Message size (B): " << msg_size << "\n"
                << "Total time (s): " << total_time_s << "\n"
                << "Latency (us): " << total_time_s * 1e6 / config.niters
                << "\n"
                << "Message Rate (K/s): " << msg_rate / 1e3 << "\n"
                << "Bandwidth (MB/s): " << bandwidth / 1e6 << std::endl;
    }
  }
  lcw::free_cq(scq);
  if (config.op == lcw::op_t::SEND) lcw::free_cq(rcq);
}

void progress_thread_fn(int progress_id)
{
  LCT_tbarrier_arrive_and_wait(tbarrier_all);
  assert(config.nprgthreads <= devices.size());
  int ndevices_per_prgthread =
      (devices.size() + config.nprgthreads - 1) / config.nprgthreads;
  while (!progress_thread_stop) {
    for (int i = 0; i < ndevices_per_prgthread; ++i) {
      int device_idx = progress_id * ndevices_per_prgthread + i;
      if (device_idx >= devices.size()) break;
      lcw::do_progress(devices[device_idx].device);
    }
  }
}

int main(int argc, char* argv[])
{
  LCT_args_parser_t argsParser = LCT_args_parser_alloc();
  LCT_dict_str_int_t dict[] = {{"send", (int)lcw::op_t::SEND},
                               {"put", (int)lcw::op_t::PUT}};
  LCT_args_parser_add_dict(argsParser, "op", required_argument,
                           (int*)&config.op, dict, 2);
  LCT_args_parser_add(argsParser, "ndevices", required_argument,
                      &config.ndevices);
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
  LCT_args_parser_add(argsParser, "send-window", required_argument,
                      &config.send_window);
  LCT_args_parser_add(argsParser, "recv-window", required_argument,
                      &config.recv_window);
  LCT_args_parser_parse(argsParser, argc, argv);

  lcw::initialize();
  if (lcw::get_rank() == 0) LCT_args_parser_print(argsParser, true);
  LCT_args_parser_free(argsParser);

  devices.resize(config.ndevices);

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
    int nthreads_per_prg = 0;
    if (config.nprgthreads != 0)
      nthreads_per_prg = config.nthreads / config.nprgthreads;
    for (int i = 0; i < config.nthreads; ++i) {
      if (config.nprgthreads > 0 && i % nthreads_per_prg == 0) {
        // spawn a progress thread
        std::thread t(progress_thread_fn, progress_pool.size());
        if (config.pin_thread) set_affinity(t.native_handle(), i % NPROCESSORS);
        progress_pool.push_back(std::move(t));
      } else {
        // spawn a worker thread
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
  for (auto& device : devices) {
    lcw::free_device(device.device);
    if (config.op == lcw::op_t::PUT) lcw::free_cq(device.put_cq);
  }
  lcw::finalize();
  return EXIT_SUCCESS;
}