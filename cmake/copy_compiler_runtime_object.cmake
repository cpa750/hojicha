if(NOT COMPILER)
  message(FATAL_ERROR "COMPILER is required")
endif()
if(NOT OBJECT_NAME)
  message(FATAL_ERROR "OBJECT_NAME is required")
endif()
if(NOT OUTPUT)
  message(FATAL_ERROR "OUTPUT is required")
endif()

execute_process(
  COMMAND "${COMPILER}" -print-file-name=${OBJECT_NAME}
  OUTPUT_VARIABLE object_path
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)

if(NOT EXISTS "${object_path}")
  message(FATAL_ERROR "Could not find ${OBJECT_NAME}; compiler returned ${object_path}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${object_path}" "${OUTPUT}"
  COMMAND_ERROR_IS_FATAL ANY
)
