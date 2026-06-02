file(GLOB USERSPACE_SOURCES CONFIGURE_DEPENDS *.c)

foreach(source ${USERSPACE_SOURCES})
  get_filename_component(program "${source}" NAME_WE)
  add_executable("userspace_${program}" crt0.s "${source}")
  set_target_properties("userspace_${program}" PROPERTIES
    OUTPUT_NAME "${program}"
    SUFFIX ".elf"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/bin"
  )
  target_compile_options_for_language("userspace_${program}" C ${HOJICHA_LIBC_CFLAGS} -O2)
  target_compile_options_for_language("userspace_${program}" ASM ${HOJICHA_LIBC_CFLAGS} -O2)
  target_include_directories("userspace_${program}" PRIVATE "${HOJICHA_PROJECT_ROOT}/libc/include")
  target_link_options("userspace_${program}" PRIVATE
    -nostdlib
    -static
    -no-pie
    -Wl,-melf_x86_64
    -Wl,-e,_start
  )
  target_link_libraries("userspace_${program}" PRIVATE hojicha_libc gcc)
endforeach()
