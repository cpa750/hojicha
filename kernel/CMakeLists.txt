add_library(kernel_objects OBJECT)
target_compile_options_for_language(kernel_objects C ${HOJICHA_KERNEL_CFLAGS})
target_compile_options_for_language(kernel_objects ASM_NASM -Wall -g)
target_include_directories(kernel_objects PRIVATE include "${HOJICHA_PROJECT_ROOT}/libc/include")

if(HOJICHA_TEST_ALL)
  set(HOJICHA_TEST_KMALLOC ON)
  set(HOJICHA_TEST_INITRD ON)
  set(HOJICHA_TEST_VFS ON)
  set(HOJICHA_TEST_CHARDEV ON)
  set(HOJICHA_TEST_RINGBUFFER ON)
endif()

set(HOJICHA_KERNEL_DEFINES __is_kernel LIMINE_API_REVISION=3)
if(HOJICHA_TEST_KMALLOC)
  list(APPEND HOJICHA_KERNEL_DEFINES __test_kmalloc)
endif()
if(HOJICHA_TEST_INITRD)
  list(APPEND HOJICHA_KERNEL_DEFINES __test_initrd)
endif()
if(HOJICHA_TEST_VFS)
  list(APPEND HOJICHA_KERNEL_DEFINES __test_vfs)
endif()
if(HOJICHA_TEST_CHARDEV)
  list(APPEND HOJICHA_KERNEL_DEFINES __test_chardev)
endif()
if(HOJICHA_TEST_RINGBUFFER)
  list(APPEND HOJICHA_KERNEL_DEFINES __test_ringbuffer)
endif()
if(HOJICHA_HLOG_LEVEL)
  list(APPEND HOJICHA_KERNEL_DEFINES "DEFAULT_HLOG_LEVEL=${HOJICHA_HLOG_LEVEL}")
endif()
target_compile_definitions(kernel_objects PRIVATE ${HOJICHA_KERNEL_DEFINES})

add_subdirectory(arch)
add_subdirectory(cpu)
add_subdirectory(dev)
add_subdirectory(drivers)
add_subdirectory(fonts)
add_subdirectory(fs)
add_subdirectory(kernel)
add_subdirectory(memory)
add_subdirectory(multitask)
add_subdirectory(utils)

add_compiler_runtime_object(crtbegin.o CRTBEGIN_OBJECT)
add_compiler_runtime_object(crtend.o CRTEND_OBJECT)

add_executable(hojicha_kernel
  arch/${HOJICHA_ARCH}/crti.s
  ${CRTBEGIN_OBJECT}
  $<TARGET_OBJECTS:kernel_objects>
  ${CRTEND_OBJECT}
  arch/${HOJICHA_ARCH}/crtn.s
)
set_target_properties(hojicha_kernel PROPERTIES
  OUTPUT_NAME hojicha.kernel
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/arch/${HOJICHA_ARCH}/linker.ld"
)
target_compile_options_for_language(hojicha_kernel ASM ${HOJICHA_KERNEL_CFLAGS})
target_link_options(hojicha_kernel PRIVATE
  -T "${CMAKE_CURRENT_LIST_DIR}/arch/${HOJICHA_ARCH}/linker.ld"
  -nostdlib
  -static
  -Wl,-melf_x86_64
  -Wl,-nostdlib
  -Wl,-static
  -Wl,-zmax-page-size=0x1000
  -Wl,--gc-sections
)
target_link_libraries(hojicha_kernel PRIVATE hojicha_libk gcc)

install(DIRECTORY include/ DESTINATION include COMPONENT headers)
install(TARGETS hojicha_kernel RUNTIME DESTINATION /boot COMPONENT kernel)
