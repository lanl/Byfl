############################################################################
# This file defines some helper functions needed by the Byfl build process #
############################################################################

# Wrap execute_process with actions appropriate to the Byfl build process.
#
# Required keyword arguments: OUTPUT_VARIABLE MESSAGE COMMAND
function(set_var_to_process_output)
  # Parse the function options.
  set(oneValueArgs OUTPUT_VARIABLE MESSAGE)
  set(multiValueArgs COMMAND)
  cmake_parse_arguments(PARSE_ARGV 0 EXEC "" "${oneValueArgs}" "${multiValueArgs}")

  # Execute the command.
  message(STATUS ${EXEC_MESSAGE})  
  execute_process(COMMAND ${EXEC_COMMAND}
    RESULT_VARIABLE _exec_result
    OUTPUT_VARIABLE _exec_output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

  # Set the output variable only if the command succeeded.
  if (_exec_result EQUAL 0)
    message(STATUS "${EXEC_MESSAGE} - ${_exec_output}")
    set(${OUTPUT_VARIABLE} ${_exec_output} PARENT_SCOPE)
  else (_exec_result EQUAL 0)
    message(STATUS "${EXEC_MESSAGE} - failed")
  endif (_exec_result EQUAL 0)
endfunction(set_var_to_process_output)
