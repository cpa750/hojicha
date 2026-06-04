set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_MINSIZEREL "" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_ASM_NASM_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_ASM_NASM_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_ASM_NASM_FLAGS_RELEASE "" CACHE STRING "" FORCE)

set(HOJICHA_HOST "x86_64-elf" CACHE STRING "Target compiler triplet")
set(HOJICHA_SYSROOT "${HOJICHA_PROJECT_ROOT}/sysroot" CACHE PATH "Target sysroot")
set(HOJICHA_ARCH "" CACHE STRING "Target architecture")
set(HOJICHA_BUILD_FLAVOR "release" CACHE STRING "Artifact build flavor")
set(HOJICHA_DEBUG_QEMU OFF CACHE BOOL "Enable QEMU debug build flags")
set(HOJICHA_TEST_KMALLOC OFF CACHE BOOL "Enable kmalloc boot tests")
set(HOJICHA_TEST_INITRD OFF CACHE BOOL "Enable initrd boot tests")
set(HOJICHA_TEST_VFS OFF CACHE BOOL "Enable VFS boot tests")
set(HOJICHA_TEST_CHARDEV OFF CACHE BOOL "Enable chardev boot tests")
set(HOJICHA_TEST_RINGBUFFER OFF CACHE BOOL "Enable ringbuffer boot tests")
set(HOJICHA_TEST_ALL OFF CACHE BOOL "Enable all boot tests")
set(HOJICHA_HLOG_LEVEL "" CACHE STRING "Default hlog level macro")

if(NOT HOJICHA_ARCH)
  if(HOJICHA_HOST MATCHES "^i[0-9]86-")
    set(HOJICHA_ARCH i386)
  else()
    string(REGEX MATCH "^[A-Za-z0-9_]+" HOJICHA_ARCH "${HOJICHA_HOST}")
  endif()
endif()

if(NOT HOJICHA_ARCH STREQUAL "x86_64")
  message(FATAL_ERROR "The CMake migration currently supports x86_64 only; got ${HOJICHA_ARCH}")
endif()

set(HOJICHA_COMMON_CFLAGS
  -std=gnu11
  -ffreestanding
  -fno-stack-protector
  -fno-stack-check
  -m64
  -march=x86-64
  -mno-80387
  -mno-mmx
  -mno-sse
  -mno-sse2
  -fno-omit-frame-pointer
  -mno-red-zone
  -mcmodel=large
  -Dh64
  -Wall
  -Wextra
)

if(HOJICHA_HOST MATCHES "-elf($|-)")
  list(APPEND HOJICHA_COMMON_CFLAGS -isystem "${HOJICHA_SYSROOT}/usr/include")
endif()

set(HOJICHA_LIBC_CFLAGS
  ${HOJICHA_COMMON_CFLAGS}
  -O3
  -fno-pie
  -fno-pic
)

set(HOJICHA_KERNEL_CFLAGS
  ${HOJICHA_COMMON_CFLAGS}
  -O3
  -fPIE
)

if(HOJICHA_DEBUG_QEMU)
  list(APPEND HOJICHA_KERNEL_CFLAGS -g -D__debug_virtual)
  list(APPEND HOJICHA_LIBC_CFLAGS -g)
endif()

function(target_compile_options_for_language target language)
  foreach(option IN LISTS ARGN)
    target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:${language}>:${option}>")
  endforeach()
endfunction()

function(add_compiler_runtime_object object_name output_var)
  set(output "${CMAKE_CURRENT_BINARY_DIR}/${object_name}")
  add_custom_command(
    OUTPUT "${output}"
    COMMAND
      "${CMAKE_COMMAND}"
      -DCOMPILER=${CMAKE_C_COMPILER}
      -DOBJECT_NAME=${object_name}
      -DOUTPUT=${output}
      -P "${HOJICHA_PROJECT_ROOT}/cmake/copy_compiler_runtime_object.cmake"
    DEPENDS "${HOJICHA_PROJECT_ROOT}/cmake/copy_compiler_runtime_object.cmake"
    VERBATIM
  )
  set_source_files_properties("${output}" PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
  set(${output_var} "${output}" PARENT_SCOPE)
endfunction()
