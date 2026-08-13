file(GLOB USERSPACE_SOURCES CONFIGURE_DEPENDS *.c)

set(HOJICHA_USERSPACE_BINARY_DIR "${CMAKE_CURRENT_LIST_DIR}/bin")

function(add_hojicha_userspace_executable target output_name)
  add_executable(
    "${target}"
    "${HOJICHA_PROJECT_ROOT}/libc/internal/crt0.s"
    ${ARGN}
  )
  set_target_properties("${target}" PROPERTIES
    OUTPUT_NAME "${output_name}"
    SUFFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${HOJICHA_USERSPACE_BINARY_DIR}"
  )
  target_compile_options_for_language("${target}" C ${HOJICHA_LIBC_CFLAGS} -O2)
  target_compile_options_for_language("${target}" ASM ${HOJICHA_LIBC_CFLAGS} -O2)
  target_compile_definitions("${target}" PRIVATE __HOJICHA__)
  target_include_directories("${target}" PRIVATE "${HOJICHA_PROJECT_ROOT}/libc/include")
  target_link_options("${target}" PRIVATE
    -nostdlib
    -static
    -no-pie
    -Wl,-melf_x86_64
    -Wl,-e,_start
  )
  target_link_libraries("${target}" PRIVATE hojicha_libc gcc)
endfunction()

foreach(source ${USERSPACE_SOURCES})
  get_filename_component(program "${source}" NAME_WE)
  add_hojicha_userspace_executable(
    "userspace_${program}"
    "${program}"
    "${source}"
  )
endforeach()

if(HOJICHA_BUILD_DOOMGENERIC)
  add_subdirectory(hojicha-doomgeneric)
endif()
