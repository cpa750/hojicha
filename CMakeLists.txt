cmake_minimum_required(VERSION 3.20)

project(hojicha C ASM ASM_NASM)

set(HOJICHA_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}")
include(cmake/HojichaBuild.cmake)

set(HOJICHA_KERNEL_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/kernel/include" CACHE PATH "Kernel headers for libk")

add_subdirectory(libc)
add_subdirectory(kernel)
add_subdirectory(userspace)

install(FILES limine.conf DESTINATION /boot/limine COMPONENT boot)
