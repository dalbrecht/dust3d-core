# SPDX-License-Identifier: MIT
# Ordinary licensed fixtures only. No limit exhaustion or OS failure injection.
foreach(required WORKER VERIFIER FIXTURES SCRATCH RUNTIME)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing resource process argument: ${required}")
    endif()
endforeach()
if(NOT RUNTIME STREQUAL "ordinary" AND NOT RUNTIME STREQUAL "asan")
    message(FATAL_ERROR "Unsupported compiled resource runtime")
endif()
execute_process(COMMAND "${VERIFIER}" prepare "${FIXTURES}" "${SCRATCH}"
    RESULT_VARIABLE prepared TIMEOUT 30)
if(NOT prepared EQUAL 0)
    message(FATAL_ERROR "Could not prepare ordinary resource fixtures: ${prepared}")
endif()
# Explicit test budgets, not production defaults. No virtual-address cap for ASan.
set(arguments --resource-profile-v1 "${RUNTIME}" 600 600 16777216 16777216 128 128)
foreach(scenario resourceCapsule resourceSword)
    execute_process(COMMAND "${WORKER}" ${arguments}
        INPUT_FILE "${SCRATCH}/${scenario}.request"
        OUTPUT_FILE "${SCRATCH}/${scenario}.reply"
        ERROR_FILE "${SCRATCH}/${scenario}.error"
        RESULT_VARIABLE status TIMEOUT 30)
    if(NOT "${status}" STREQUAL "0")
        message(FATAL_ERROR "Ordinary resource worker ${scenario} did not succeed: ${status}")
    endif()
    execute_process(COMMAND "${VERIFIER}" verifyResource "${FIXTURES}" "${SCRATCH}" "${scenario}" "${RUNTIME}"
        RESULT_VARIABLE verified TIMEOUT 30)
    if(NOT verified EQUAL 0)
        message(FATAL_ERROR "Resource state/result verification failed for ${scenario}: ${verified}")
    endif()
endforeach()
# Invalid launch metadata must fail before archive handling or limit installation.
# No low cap is installed and no active limit is deliberately exceeded.
foreach(scenario version runtime zero number address)
    set(rejected ${arguments})
    if(scenario STREQUAL "version")
        list(REMOVE_AT rejected 0)
        list(PREPEND rejected --resource-profile-v2)
    elseif(scenario STREQUAL "runtime")
        list(REMOVE_AT rejected 1)
        if(RUNTIME STREQUAL "ordinary")
            list(INSERT rejected 1 asan)
        else()
            list(INSERT rejected 1 ordinary)
        endif()
    elseif(scenario STREQUAL "zero")
        list(REMOVE_AT rejected 2)
        list(INSERT rejected 2 0)
    elseif(scenario STREQUAL "address")
        list(APPEND rejected 17179869184 17179869184)
    else()
        list(REMOVE_AT rejected 2)
        list(INSERT rejected 2 1x)
    endif()
    execute_process(COMMAND "${WORKER}" ${rejected}
        INPUT_FILE "${SCRATCH}/shortHeader.request"
        OUTPUT_FILE "${SCRATCH}/rejected-${scenario}.reply"
        ERROR_FILE "${SCRATCH}/rejected-${scenario}.error"
        RESULT_VARIABLE status TIMEOUT 30)
    file(SIZE "${SCRATCH}/rejected-${scenario}.reply" reply_size)
    file(SIZE "${SCRATCH}/rejected-${scenario}.error" error_size)
    file(READ "${SCRATCH}/rejected-${scenario}.error" cause LIMIT 1024)
    if(NOT "${status}" STREQUAL "64" OR NOT reply_size EQUAL 0 OR error_size GREATER 1024)
        message(FATAL_ERROR "Invalid resource launch ${scenario} did not fail before archive processing")
    endif()
    if(NOT cause MATCHES "stage=(usage|resource-policy)")
        message(FATAL_ERROR "Invalid resource launch ${scenario} lost its original bounded cause")
    endif()
endforeach()
