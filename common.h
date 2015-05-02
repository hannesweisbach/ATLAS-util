#pragma once

#include <type_traits>
#include <string>
#include <iostream>
#include <initializer_list>

#include <cstring>
#include <cerrno>
#include <ctime>
#include <csignal>

template <typename T> void check_zero(T ret, std::string msg = "") {
  static_assert(std::is_integral<T>::value,
                "Return value check only for integers.");
  if (ret) {
    std::cerr << msg << " (" << errno << "): " << strerror(errno) << std::endl;
    std::terminate();
  }
}

struct timespec operator-(const struct timespec &lhs,
                          const struct timespec &rhs);
pid_t gettid();
void set_affinity(unsigned cpu = 0, pid_t tid = gettid());
void set_affinity(std::initializer_list<unsigned> cpus, pid_t tid = gettid());

using signal_handler_t = void (*)(int, siginfo_t *, void *);
void set_deadline_handler(signal_handler_t handler);
void wait_for_deadline(); 

