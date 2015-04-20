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

add_library(common STATIC common.c++)
add_executable(cputime cputime.c++)
target_link_libraries(cputime common ${CMAKE_THREAD_LIBS_INIT})

add_executable(syscall syscall.c++)

