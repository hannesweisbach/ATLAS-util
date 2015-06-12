cmake_minimum_required (VERSION 2.8.11)
project(ATLAS-Tests)

#  -std=c++14 -stdlib=libc++ common.c++ cpu_time.cc -pthread -o cputime
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	message(FATAL_ERROR "Clang as compiler required")
endif()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-c++98-compat-pedantic")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-global-constructors")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-exit-time-destructors")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-padded")

find_package(Threads)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost 1.54.0 COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIR})

add_library(common STATIC common.c++)
add_executable(cputime cputime.c++)
target_link_libraries(cputime common ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(zero_exec zero_exec.c++)
target_link_libraries(zero_exec common ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(producer-consumer producer-consumer.c++)
target_link_libraries(producer-consumer common ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(fork fork.c++)
target_link_libraries(fork common)

add_executable(overrun overrun.c++)
target_link_libraries(overrun common ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(blocking blocking.c++)
target_link_libraries(blocking common ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(submit submit.c++)
target_link_libraries(submit common ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(next next.c++)
target_link_libraries(next common ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(delete delete.c++)
target_link_libraries(delete common ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(overlap overlap.c++)
target_link_libraries(overlap common ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES})

add_executable(syscall syscall.c++)

