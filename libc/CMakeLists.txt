cmake_minimum_required(VERSION 3.20)

project(hojicha_libc C ASM_NASM)

if(NOT DEFINED HOJICHA_PROJECT_ROOT)
  set(HOJICHA_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
  include("${HOJICHA_PROJECT_ROOT}/cmake/HojichaBuild.cmake")
endif()

if(NOT DEFINED HOJICHA_KERNEL_INCLUDE_DIR)
  set(HOJICHA_KERNEL_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../kernel/include" CACHE PATH "Kernel headers for libk")
endif()

add_library(libc_objects OBJECT)
target_compile_options_for_language(libc_objects C ${HOJICHA_LIBC_CFLAGS})
target_compile_options_for_language(libc_objects ASM_NASM -Wall -g)
target_compile_definitions(libc_objects PRIVATE __is_libc)
target_include_directories(libc_objects PRIVATE include)

add_library(libk_objects OBJECT)
target_compile_options_for_language(libk_objects C ${HOJICHA_LIBC_CFLAGS})
target_compile_options_for_language(libk_objects ASM_NASM -Wall -g)
target_compile_definitions(libk_objects PRIVATE __is_libc __is_libk __printf_serial)
target_include_directories(libk_objects PRIVATE include "${HOJICHA_KERNEL_INCLUDE_DIR}")
if(HOJICHA_STRESS_HMALLOC)
  target_compile_definitions(libk_objects PRIVATE __stress_hmalloc)
endif()

add_subdirectory(arch)
add_subdirectory(internal)
add_subdirectory(ctype)
add_subdirectory(dirent)
add_subdirectory(fcntl)
add_subdirectory(hmalloc)
add_subdirectory(stdio)
add_subdirectory(stdlib)
add_subdirectory(string)
add_subdirectory(sys)
add_subdirectory(time)
add_subdirectory(unistd)

target_sources(libc_objects PRIVATE errno.c)
target_sources(libk_objects PRIVATE errno.c)

add_library(hojicha_libc STATIC $<TARGET_OBJECTS:libc_objects>)
set_target_properties(hojicha_libc PROPERTIES OUTPUT_NAME c)

add_library(hojicha_libk STATIC $<TARGET_OBJECTS:libk_objects>)
set_target_properties(hojicha_libk PROPERTIES OUTPUT_NAME k)

install(DIRECTORY include/ DESTINATION include COMPONENT headers)
install(TARGETS hojicha_libc hojicha_libk ARCHIVE DESTINATION lib COMPONENT libs)
