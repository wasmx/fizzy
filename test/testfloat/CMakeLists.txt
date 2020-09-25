# Fizzy: A fast WebAssembly interpreter
# Copyright 2020 The Fizzy Authors.
# SPDX-License-Identifier: Apache-2.0

option(FIZZY_TESTFLOAT_REQUIRED "The testfloat_gen tool is expected to be found" FALSE)

if(FIZZY_TESTFLOAT_REQUIRED OR "$ENV{FIZZY_TESTFLOAT_REQUIRED}")
    set(TESTFLOAT_REQUIRED TRUE)
endif()

add_executable(fizzy-testfloat testfloat.cpp)
target_link_libraries(fizzy-testfloat PRIVATE fizzy::fizzy fizzy::test-utils)
target_include_directories(fizzy-testfloat PRIVATE ${PROJECT_SOURCE_DIR}/lib)

find_program(TESTFLOAT_GEN testfloat_gen)
if(TESTFLOAT_GEN)
    # Testing level 1 is default. Testing level 2 generates much more test cases,
    # f64 binop tests will take ~100 seconds for Release build.
    set(TESTFLOAT_LEVEL 1 CACHE STRING "testfloat_gen testing level (1 or 2)")

    string(RANDOM LENGTH 10 ALPHABET 0123456789 TESTFLOAT_SEED)
    set(TESTFLOAT_SEED ${TESTFLOAT_SEED} CACHE INTERNAL "testfloat_get seed")  # Save in cache for lookup.
    message(VERBOSE "testfloat_gen seed: ${TESTFLOAT_SEED}")

    set(IGNORE_LIST)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # TODO: Clang produces -0 for input 0, see https://bugs.llvm.org/show_bug.cgi?id=47393.
        list(APPEND IGNORE_LIST ui64_to_f64/min)
    endif()

    set(ROUNDING_MODES near_even minMag min max)

    set(
        ROUNDING_DEPENDENT_FUNCTIONS
        f32_sqrt f64_sqrt
        f32_add f64_add
        f32_sub f64_sub
        f32_mul f64_mul
        f32_div f64_div

        i32_to_f32 ui32_to_f32 i64_to_f32 ui64_to_f32 i64_to_f64 ui64_to_f64
        f64_to_f32
    )

    set(
        ROUNDING_INDEPENDENT_FUNCTIONS
        f32_eq f64_eq f32_lt f64_lt f32_le f64_le

        # Always exact -> no rounding
        i32_to_f64 ui32_to_f64
        f32_to_f64
    )

    set(
        TRUNC_FUNCTIONS
        f32_trunc_to_i32 f32_trunc_to_ui32 f32_trunc_to_i64 f32_trunc_to_ui64 f64_trunc_to_i32 f64_trunc_to_ui32 f64_trunc_to_i64 f64_trunc_to_ui64
    )

    # Here we need to map Wasm function to testfloat_gen arguments as Wasm has multiple functions
    # with non-sticky rounding direction while testfloat_gen has single function and rounding
    # direction options.
    #
    # TODO: NaNs are ignored for these as Fizzy always return nan:canonical.
    set(
        ROUND_TO_INT_FUNCTIONS
        "f32_ceil\;-rmax f32_roundToInt"
        "f32_floor\;-rmin f32_roundToInt"
        "f32_trunc\;-rminMag f32_roundToInt"
        "f32_nearest\;-rnear_even f32_roundToInt"
        "f64_ceil\;-rmax f64_roundToInt"
        "f64_floor\;-rmin f64_roundToInt"
        "f64_trunc\;-rminMag f64_roundToInt"
        "f64_nearest\;-rnear_even f64_roundToInt"
    )


    macro(add_testfloat_test NAME TESTFLOAT_GEN_ARGS FIZZY_TESTFLOAT_ARGS)
        if(NOT ${NAME} IN_LIST IGNORE_LIST)
            add_test(
                NAME fizzy/testfloat/${NAME}
                COMMAND bash -c "set -o pipefail; ${TESTFLOAT_GEN} -seed ${TESTFLOAT_SEED} -level ${TESTFLOAT_LEVEL} ${TESTFLOAT_GEN_ARGS} | $<TARGET_FILE:fizzy-testfloat> ${FIZZY_TESTFLOAT_ARGS}"
            )
            set_tests_properties(
                fizzy/testfloat/${NAME}
                PROPERTIES
                ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/testfloat-%p.profraw
            )
        endif()
    endmacro()

    foreach(FUNCTION ${ROUNDING_DEPENDENT_FUNCTIONS})
        foreach(ROUNDING_MODE ${ROUNDING_MODES})
            add_testfloat_test(${FUNCTION}/${ROUNDING_MODE} "-r${ROUNDING_MODE} ${FUNCTION}" "-r${ROUNDING_MODE} ${FUNCTION}")
        endforeach()
    endforeach()

    foreach(FUNCTION ${ROUNDING_INDEPENDENT_FUNCTIONS})
        foreach(ROUNDING_MODE ${ROUNDING_MODES})
            add_testfloat_test(${FUNCTION}/${ROUNDING_MODE} "${FUNCTION}" "-r${ROUNDING_MODE} ${FUNCTION}")
        endforeach()
    endforeach()

    foreach(FUNCTION ${TRUNC_FUNCTIONS})
        string(REPLACE "_trunc" "" TESTFLOAT_GEN_FUNCTION ${FUNCTION})
        foreach(ROUNDING_MODE ${ROUNDING_MODES})
            add_testfloat_test(${FUNCTION}/${ROUNDING_MODE} "-rminMag ${TESTFLOAT_GEN_FUNCTION}" "-r${ROUNDING_MODE} ${FUNCTION}")
        endforeach()
    endforeach()

    foreach(FUNCTION_PAIR ${ROUND_TO_INT_FUNCTIONS})
        list(GET FUNCTION_PAIR 0 FUNCTION)
        list(GET FUNCTION_PAIR 1 TESTFLOAT_GEN_ARGS)
        foreach(ROUNDING_MODE ${ROUNDING_MODES})
            add_testfloat_test(${FUNCTION}/${ROUNDING_MODE} "${TESTFLOAT_GEN_ARGS}" "-r${ROUNDING_MODE} -ignoreNaNs ${FUNCTION}")
        endforeach()
    endforeach()

elseif(TESTFLOAT_REQUIRED)
    message(FATAL_ERROR "testfloat_gen tool not found")
endif()


# fizzy-testfloat selftests

function(add_testfloat_selftest NAME ARGS INPUT EXPECTED_ERROR)
    if(INPUT)
        set(command sh -c "echo ${INPUT} | $<TARGET_FILE:fizzy-testfloat> ${ARGS}")
    else()
        separate_arguments(ARGS)
        set(command $<TARGET_FILE:fizzy-testfloat> ${ARGS})
    endif()

    add_test(
        NAME fizzy/testfloat/selftest/${NAME}
        COMMAND ${command}
    )

    if(EXPECTED_ERROR)
        set(pass_regular_expression PASS_REGULAR_EXPRESSION ${EXPECTED_ERROR})
    endif()
    set_tests_properties(
        fizzy/testfloat/selftest/${NAME}
        PROPERTIES
        ${pass_regular_expression}
        ENVIRONMENT LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/testfloat-selftest-%p.profraw
    )
endfunction()

add_testfloat_selftest(missing_function_argument "" "" "ERROR: missing <function> argument")
add_testfloat_selftest(invalid_function "XYZ" "" "ERROR: unknown <function>: XYZ")
add_testfloat_selftest(unknown_option "-abc" "" "ERROR: unknown option: -abc")
add_testfloat_selftest(unexpected_argument "f32_add zZz" "" "ERROR: unexpected argument: zZz")
add_testfloat_selftest(invalid_input "f32_add" "X" "ERROR: invalid input format")
add_testfloat_selftest(correct_input "f32_add" "00000000 00000000 00000000 00" "")
add_testfloat_selftest(result_failure_1 "i32_to_f32" "00000000 00000001 00" "FAILURE: 00000000 <- 00000000\n         00000001 \\(expected\\)")
add_testfloat_selftest(result_failure_2 "i64_to_f64" "0000000000000000 8000000000000001 00" "FAILURE: 0000000000000000 <- 0000000000000000\n         8000000000000001 \\(expected\\)")
add_testfloat_selftest(result_failure_3 "f32_add" "00000000 00000000 00000001 00" "FAILURE: 00000000 <- 00000000 00000000\n         00000001 \\(expected\\)")
add_testfloat_selftest(unexpected_trap "f32_trunc_to_i32" "7F7FFFFF 00000000 00" "FAILURE: 1 <- 7F7FFFFF\n         0 \\(expected\\)")
add_testfloat_selftest(missing_trap "f32_trunc_to_i32" "00000000 00000000 10" "FAILURE: 0 <- 00000000\n         1 \\(expected\\)")
add_testfloat_selftest(unexpected_signaling_nan_f32 "-ignoreNaNs f32_add" "7FA00000 7FA00000 7FA00000 00" "ERROR: invalid input: unexpected signaling NaN")
add_testfloat_selftest(unexpected_signaling_nan_f64 "-ignoreNaNs f64_add" "7FF4000000000000 7FF4000000000000 7FF4000000000000 00" "ERROR: invalid input: unexpected signaling NaN")
