#include <vector>
#include <thread>
#include <chrono>
#include <memory>
#include <iterator>
#include <algorithm>
#include <pthread.h>
#include <utility>
#include <atomic>
#include <random>

#include <signal.h>

#include "atlas.h"
#include "common.h"

class barrier {
  mutable std::atomic<size_t> count;

public:
  barrier(size_t size) : count(size) {}
  void reset(const size_t size) { count.store(size); }
  void wait() const {
    --count;
    while (count)
      ;
  }
};

static std::atomic_bool running__{true};

static void sig_term(int, siginfo_t *, void *) { running__ = false; }

namespace continuous {
/* producer and consumer continuously submitting and processing work */
static void producer(const std::vector<std::unique_ptr<std::thread>> &consumers,
                     const barrier &barrier, const size_t samples,
                     const std::pair<size_t, size_t> producer_info,
                     std::unique_ptr<std::atomic<size_t>[]> &queue,
                     std::atomic_bool &running) {
  using namespace std::chrono;

  const size_t producer_id = producer_info.first;
  const size_t num_producers = producer_info.second;
  const size_t num_consumers = consumers.size();
  size_t consumer_id;

  auto cpus = std::thread::hardware_concurrency();
  if (cpus > 3) {
    set_affinity({1, 2});
  } else {
    set_affinity({0});
  }

  barrier.wait();

  /* Note: 4-core execution time + deadline: 1s */

  for (size_t sample = 0; running; ++sample) {
    consumer_id = 0;
    for (const auto &consumer : consumers) {
      if (queue[consumer_id] < samples) {
        uint64_t id = num_producers * num_consumers * sample +
                      num_consumers * producer_id + consumer_id;
        ++queue[consumer_id];
        check_zero(atlas::np::submit(consumer->get_id(), id, 5000ms, 5000ms));
      }
      ++consumer_id;
    }
  }
}

static void consumer(const barrier &barrier, std::atomic<size_t> &samples,
                     std::atomic_bool &running) {
  unsigned nr_cpus = std::thread::hardware_concurrency();
  set_affinity(nr_cpus - 1);

  barrier.wait();

  for (; running || samples;) {
    check_zero(atlas::next());
    --samples;
    /* do some work */
    for (size_t i = 0; i < 1000 * 1000; ++i)
      __asm__ __volatile__("nop");
  }
}
}

namespace miss {
/* producer and consumer, where consumer sometimes misses deadlines */
static void producer(std::thread::id consumer, const barrier &barrier,
                     std::atomic_bool &running, std::atomic_bool &in_next,
                     std::atomic<size_t> &samples) {
  using namespace std::chrono;

  uint64_t id = 0;
  std::mt19937_64 generator;
  std::uniform_int_distribution<bool> blocking;

  auto cpus = std::thread::hardware_concurrency();
  if (cpus > 3) {
    set_affinity({1, 2});
  } else {
    set_affinity({0});
  }

  barrier.wait();

  std::this_thread::sleep_for(1s);

  for (; running;) {
    if (blocking(generator)) {
      while (samples || !in_next)
        ;
      std::this_thread::sleep_for(1ms);
      ++samples;
      atlas::np::submit(consumer, id++, 90ms, 100ms);
    } else {
      while (samples >= 10)
        ;
      ++samples;
      atlas::np::submit(consumer, id++, 90ms, 100ms);
    }
  }
}

static void consumer(const barrier &barrier, std::atomic_bool &running,
                     std::atomic_bool &in_next, std::atomic<size_t> &samples) {
  std::mt19937_64 generator;
  std::uniform_int_distribution<bool> miss;
  unsigned nr_cpus = std::thread::hardware_concurrency();
  set_affinity(nr_cpus - 1);

  barrier.wait();

  for (; running || samples;) {
    in_next = true;
    check_zero(atlas::next());
    --samples;
    in_next = false;

    if (miss(generator)) {
      wait_for_deadline();
    } else {
      for (size_t i = 0; i < 1000 * 1000; ++i)
        __asm__ __volatile__("nop");
    }
  }
}
}

static void producer(const std::vector<std::unique_ptr<std::thread>> &consumers,
                     const barrier &barrier, const size_t samples,
                     const std::pair<size_t, size_t> producer_info) {
  using namespace std::chrono;

  const size_t producer_id = producer_info.first;
  const size_t num_producers = producer_info.second;
  const size_t num_consumers = consumers.size();
  size_t consumer_id;

  auto cpus = std::thread::hardware_concurrency();
  if (cpus > 3) {
    set_affinity({1, 2});
  } else {
    set_affinity({0});
  }

  barrier.wait();

  /* Note: 4-core execution time + deadline: 1ms */
  consumer_id = 0;
  for (const auto &consumer : consumers) {
    uint64_t id = num_consumers * producer_id + consumer_id;
    check_zero(atlas::np::submit(consumer->get_id(), id, 5000ms, 5000ms));
    ++consumer_id;
  }

  std::this_thread::sleep_for(1s);

  for (size_t sample = 1; sample < samples; ++sample) {
    consumer_id = 0;
    for (const auto &consumer : consumers) {
      uint64_t id = num_producers * num_consumers * sample +
                    num_consumers * producer_id + consumer_id;
      /*timeval.tv_usec += 1;*/
      check_zero(atlas::np::submit(consumer->get_id(), id, 5000ms, 5000ms));
      ++consumer_id;
    }
  }
}

static void consumer(const barrier &barrier, const size_t samples) {
  unsigned nr_cpus = std::thread::hardware_concurrency();
  set_affinity(nr_cpus - 1);

  barrier.wait();

  for (size_t sample = 0; sample < samples; ++sample) {
    check_zero(atlas::next());
    /* do some work */
    for (size_t i = 0; i < 1000 * 1000; ++i)
      __asm__ __volatile__("nop");
  }
}

int main() {
  bool continuous = false;
  bool miss = true;
  size_t num_consumers = 3;
  size_t num_producers = 10;
  size_t samples = 1000;

  auto queue = std::make_unique<std::atomic<size_t>[]>(num_consumers);
  std::atomic_bool in_next;

  std::vector<std::unique_ptr<std::thread>> producers;
  std::vector<std::unique_ptr<std::thread>> consumers;

  barrier barrier(num_consumers + num_producers);

  set_signal_handler(SIGTERM, sig_term);

  if (continuous) {
    for (size_t consumer = 0; consumer < num_consumers; ++consumer) {
      queue[consumer].store(0);
      consumers.emplace_back(std::make_unique<
          std::thread>([&barrier, &samples = queue[consumer] ]() {
        continuous::consumer(barrier, samples, running__);
      }));
    }

    for (size_t producer = 0; producer < num_producers; ++producer) {
      producers.push_back(std::make_unique<std::thread>([
        &consumers,
        &barrier,
        samples,
        producer_info = std::make_pair(producer, num_producers),
        &queue
      ]() {
        continuous::producer(consumers, barrier, samples, producer_info, queue,
                             running__);
      }));
    }
  } else if (miss) {
    barrier.reset(2);
    queue[0].store(0);
    consumers.emplace_back(std::make_unique<
        std::thread>([&barrier, &samples = queue[0], &in_next ]() {
      miss::consumer(barrier, running__, in_next, samples);
    }));

    producers.push_back(std::make_unique<std::thread>([
      consumer = consumers.back()->get_id(),
      &barrier,
      &in_next,
          &samples = queue[0]
    ]() { miss::producer(consumer, barrier, running__, in_next, samples); }));
  } else {
    std::generate_n(
        std::back_inserter(consumers), num_consumers,
        [&barrier, samples = samples * num_producers ]() {
          return std::make_unique<std::thread>(
              [&barrier, samples]() { consumer(barrier, samples); });
        });

    for (size_t producer = 0; producer < num_producers; ++producer) {
      producers.push_back(std::make_unique<std::thread>([
        &consumers,
        &barrier,
        samples,
        producer_info = std::make_pair(producer, num_producers)
      ]() { ::producer(consumers, barrier, samples, producer_info); }));
    }
  }

  for (auto &&producer : producers) {
    producer->join();
  }

  for (auto &&consumer : consumers) {
    consumer->join();
  }
}
