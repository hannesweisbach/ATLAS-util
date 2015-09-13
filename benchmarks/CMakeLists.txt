add_executable(balancing load_balancing.c++)
target_link_libraries(balancing Threads::Threads ${Boost_LIBRARIES} common)
