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

template <class Rep, class Period>
void busy_for(std::chrono::duration<Rep, Period> duration) {
  using namespace std::chrono;
  using duration_t = std::chrono::duration<Rep, Period>;
  auto start = high_resolution_clock::now();
  for (auto now = high_resolution_clock::now();
       duration_cast<duration_t>(now - start) < duration;
       now = high_resolution_clock::now()) {
  }
}

template <class Clock, class Duration>
void busy_until(const std::chrono::time_point<Clock, Duration> &t) {
  for (; Clock::now() < t;) {
  }
}

struct timespec operator-(const struct timespec &lhs,
                          const struct timespec &rhs);
pid_t gettid();
void set_affinity(unsigned cpu = 0, pid_t tid = gettid());
void set_affinity(std::initializer_list<unsigned> cpus, pid_t tid = gettid());

using signal_handler_t = void (*)(int, siginfo_t *, void *);
void set_signal_handler(int signal, signal_handler_t handler);
void set_deadline_handler(signal_handler_t handler);
void wait_for_deadline(); 

