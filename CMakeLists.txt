zephyr_include_directories(include)

add_subdirectory(drivers)

list(APPEND SYSCALL_INCLUDE_DIRS ${ZEPHYR_BASE}/../500e_unlock/include/drivers)
set(SYSCALL_INCLUDE_DIRS ${SYSCALL_INCLUDE_DIRS} PARENT_SCOPE)
