#include <chrono>
#include <vector>
#include <algorithm>
#include <iterator>
#include <iostream>

#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
  using namespace std::chrono;
  const size_t count = 100000;
  std::vector<uint64_t> times;

  times.reserve(count);
  std::generate_n(std::back_inserter(times), count, []() {
    auto start = high_resolution_clock::now();
    syscall(SYS_getpid);
    auto end = high_resolution_clock::now();
    return duration_cast<nanoseconds>(end - start).count();
  });

  std::sort(std::begin(times), std::end(times));
  std::cout << "Min: " << times.front() << std::endl;
  std::cout << "Mean: " << times.at(times.size() / 2) << std::endl;
  for (unsigned percentile = 95; percentile < 100; ++percentile) {
    auto idx = times.size() * percentile / 100;
    std::cout << percentile << "th: " << times.at(idx) << std::endl;
  }
  std::cout << "Max: " << times.back() << std::endl;
}
