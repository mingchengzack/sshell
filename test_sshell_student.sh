#!/bin/bash

set -o pipefail

#
# Error/warning helpers
#
die() {
    echo "Error: $*" >&2
    exit 1
}

warning() {
    echo "Warning: $*" >&2
}

#
# Scoring helpers
#
TOTAL=0
ANSWERS=()

add_answer() {
    ANSWERS+=("${1},")
}

inc_total() {
    let "TOTAL++"
}

# Returns a specific line in a multi-line string
select_line() {
    # 1: string
    # 2: line to select
    echo "$(echo "${1}" | sed "${2}q;d")"
}

fail() {
    # 1: got
    # 2: expected
    echo -e "\e[31mFail:\e[0m got '${1}' but expected '${2}'"
}

pass() {
    # got
    echo -e "\e[32mPass\e[0m: ${1}"
}

compare_output_lines() {
    # 1: output
    # 2: expected
    # 3: point step
    declare -a output_lines=("${!1}")
    declare -a expect_lines=("${!2}")
    local pts_step="${3}"

    for i in ${!output_lines[*]}; do
        if [[ "${output_lines[${i}]}" == "${expect_lines[${i}]}" ]]; then
            pass "${output_lines[${i}]}"
            sub=$(bc<<<"${sub}+${pts_step}")
        else
            fail "${output_lines[${i}]}" "${expect_lines[${i}]}" ]]
        fi
    done
}

#
# Generic function for running shell tests
#
DEFAULT_EXEC="../sshell"
sshell_test() {
    # 1: multi-line string to feed to the shell
    # 2: (optional) timeout delay (default is 2 sec)
    # 3: (optional) strace the whole test
    local cmdline="${1}"
    local delay=2
    local strace=false

    [[ $# -ge 2 ]] && delay="${2}"
    [[ $# -ge 3 ]] && strace="${3}"

    # These are global variables after the test has run so clear them out now
    unset STDOUT STDERR RET

    # Create temp files for getting stdout and stderr
    local outfile=$(mktemp)
    local errfile=$(mktemp)

    if [[ "${strace}" == false ]]; then
        # Encapsulates commands with `timeout` in case the process hangs indefinitely
        timeout "${delay}" \
            bash -c \
            "echo -e \"${cmdline}\" | ${DEFAULT_EXEC}" >${outfile} 2>${errfile}
    else
        timeout "${delay}" strace -f bash -c \
            "echo -e \"${cmdline}\" | ${DEFAULT_EXEC}" >${outfile} 2>${errfile}
    fi

    # Get the return status, stdout and stderr of the test case
    RET="${?}"
    STDOUT=$(cat "${outfile}")
    STDERR=$(cat "${errfile}")

    # Deal with the possible timeout errors
    [[ ${RET} -eq 127 ]] && die "Something is wrong (the executable probably doesn't exists)"
    [[ ${RET} -eq 124 ]] && warning "Command timed out..."

    # Clean up temp files
    rm -f "${outfile}"
    rm -f "${errfile}"
}

#
# Test cases for the simple shell
#
TEST_CASES=()

## Pre-phase 4 (exit needs to exist!)
run_exit() {
    echo -e "\n--- Running ${FUNCNAME} ---"
    sshell_test "exit\n"
    inc_total
    corr_str="Bye..."
    if [[ "${STDERR}" == "${corr_str}" ]]; then
        pass "${STDERR}"
        add_answer 1
    else
        fail "${STDERR}" "${corr_str}"
        add_answer 0
    fi
}
TEST_CASES+=("run_exit")

## Phase 2
run_cmd_no_arg() {
    echo -e "\n--- Running ${FUNCNAME} ---"
    touch titi toto
    sshell_test "ls\nexit\n"
    rm titi toto
    inc_total

    sub=0

    local line_array=()
    line_array+=("$(select_line "${STDOUT}" "2")")
    line_array+=("$(select_line "${STDOUT}" "3")")
    local corr_array=()
    corr_array+=("titi")
    corr_array+=("toto")

    compare_output_lines line_array[@] corr_array[@] "0.5"

    add_answer "${sub}"
}
TEST_CASES+=("run_cmd_no_arg")

## Phase 3
run_cmd_one_arg() {
    echo -e "\n--- Running ${FUNCNAME} ---"
    mkdir dir_test && touch dir_test/lstest
    sshell_test "ls dir_test\nexit\n"
    rm -rf dir_test
    inc_total

    sub=0

    local line_array=()
    line_array+=("$(select_line "${STDOUT}" "2")")
    local corr_array=()
    corr_array+=("lstest")

    compare_output_lines line_array[@] corr_array[@] "1"

    add_answer "${sub}"
}
TEST_CASES+=("run_cmd_one_arg")

## Phase 4
run_cd_pwd() {
    echo -e "\n--- Running ${FUNCNAME} ---"
    sshell_test "mkdir -p dir_test\ncd dir_test\npwd\nexit\n"
    rm -rf dir_test
    inc_total

    sub=0

    local line_array=()
    line_array+=("$(select_line "${STDOUT}" "4")")
    line_array+=("$(select_line "${STDERR}" "2")")
    local corr_array=()
    corr_array+=("$PWD/dir_test")
    corr_array+=("+ completed 'cd dir_test' [0]")

    compare_output_lines line_array[@] corr_array[@] "0.5"

    add_answer "${sub}"
}
TEST_CASES+=("run_cd_pwd")

## Phase 5
run_in_redir() {
    echo -e "\n--- Running ${FUNCNAME} ---"
    echo hello > t
    sshell_test "grep he < t\nexit\n"
    rm t
    inc_total

    sub=0

    local line_array=()
    line_array+=("$(select_line "${STDOUT}" "2")")
    local corr_array=()
    corr_array+=("hello")

    compare_output_lines line_array[@] corr_array[@] "1"

    add_answer "${sub}"
}
TEST_CASES+=("run_in_redir")

## Phase 6
run_out_redir() {
    echo -e "\n--- Running ${FUNCNAME} ---"
    sshell_test "echo hello > t\ncat t\nexit\n"
    rm -f t
    inc_total

    sub=0

    local line_array=()
    line_array+=("$(select_line "${STDOUT}" "3")")
    local corr_array=()
    corr_array+=("hello")

    compare_output_lines line_array[@] corr_array[@] "1"

    add_answer "${sub}"
}
TEST_CASES+=("run_out_redir")

## Phase 7
run_pipe() {
    echo -e "\n--- Running ${FUNCNAME} ---"
    sshell_test "echo hello world hello world | grep hello | wc -m\nexit\n"
    inc_total

    sub=0

    local line_array=()
    line_array+=("$(select_line "${STDOUT}" "2")")
    local corr_array=()
    corr_array+=("24")

    compare_output_lines line_array[@] corr_array[@] "1"

    add_answer "${sub}"
}
TEST_CASES+=("run_pipe")

## Phase 8
run_background() {
    echo -e "\n--- Running ${FUNCNAME} ---"
    echo "(Note: this test might hang a little)"
    sshell_test "sleep 1&\nsleep 2\nexit\n" 5
    inc_total

    sub=0

    local line_array=()
    line_array+=("$(select_line "${STDOUT}" "2")")
    line_array+=("$(select_line "${STDERR}" "1")")
    line_array+=("$(select_line "${STDERR}" "2")")
    line_array+=("$(select_line "${STDERR}" "3")")
    local corr_array=()
    corr_array+=("sshell$ sleep 2")
    corr_array+=("+ completed 'sleep 1&' [0]")
    corr_array+=("+ completed 'sleep 2' [0]")
    corr_array+=("Bye...")

    compare_output_lines line_array[@] corr_array[@] "0.25"

    add_answer "${sub}"
}
TEST_CASES+=("run_background")

#
# Main function
#
TDIR=sshell_test_dir

cleanup() {
    cd ..
    rm -rf "${TDIR}"
}

main_func() {
    # Make a new testing directory
    rm -rf "${TDIR}"
    mkdir "${TDIR}" && cd "${TDIR}"

    # Make sure that the shell executable exists
    if [[ ! -x "${DEFAULT_EXEC}" ]]; then
        cleanup
        die "Can't find shell executable"
    fi

    # Run all the tests
    for t in "${TEST_CASES[@]}"; do
        ${t}
    done

    # Display the results
    echo -e "\n---\n"
    echo "${TOTAL} test cases were passed"
    echo "${ANSWERS[*]}"

    cleanup
}

if [[ $# -eq 1 ]]; then
    DEFAULT_EXEC="../${1}"
fi
main_func
