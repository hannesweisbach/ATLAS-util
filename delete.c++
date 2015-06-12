#include <thread>
#include <iostream>
#include <cerrno>

#include <boost/program_options.hpp>

#include "atlas.h"

static uint64_t id{0};

static bool delete_nonexistent_job() {
  auto err = atlas::np::remove(std::this_thread::get_id(), ++id);

  if (err) {
    std::cout << "Expected error deleting non-existent job " << id << ":"
              << std::endl;
    std::cout << "\t" << strerror(errno) << std::endl;
  } else {
    std::cout << "Deleting non-existent job " << id << " erroneously succeeded"
              << std::endl;
  }

  return errno == EINVAL;
}

static bool delete_existent_job() {
  using namespace std::chrono;
  auto now = high_resolution_clock::now();
  atlas::np::submit(std::this_thread::get_id(), ++id, 1s, now + 1s);
  auto err = atlas::np::remove(std::this_thread::get_id(), id);

  if (err) {
    std::cout << "Deleting job " << id << " failed:" << std::endl;
    std::cout << "\t" << strerror(errno) << std::endl;
  } else {
    std::cout << "Deleting job " << id << " succeeded." << std::endl;
  }

  return errno == EINVAL;
}

int main(int argc, char *argv[]) {
  bool all;
  namespace po = boost::program_options;
  po::options_description desc("Interface tests for atlas::submit()");
  auto o = desc.add_options();
  o("help", "produce help message");
  o("nonexistent-job", "Tries to remove a job that does not exist.");
  o("existent-job", "Tries to remove a job that does exist.");
  o("all", po::value(&all)->implicit_value(true), "Run all test.");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }

  if (vm.count("nonexistent-job") || all) {
    delete_nonexistent_job();
  }

  if (vm.count("existent-job") || all) {
    delete_existent_job();
  }
}
