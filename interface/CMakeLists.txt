add_executable(np np.c++)
target_link_libraries(np common Threads::Threads)

add_executable(submit submit.c++)
target_link_libraries(submit common Threads::Threads)

add_executable(update update.c++)
target_link_libraries(update common Threads::Threads)

add_executable(delete delete.c++)
target_link_libraries(delete common Threads::Threads)


