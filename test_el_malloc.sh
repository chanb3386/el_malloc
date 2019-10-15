#!/bin/bash

DATAFILE="test_el_malloc_data.sh"

if [ ! -d "test-input" ]; then
    printf "No 'test-input' directory found, cannot proceed with tests\n"
    exit 1
fi

generate=1
run_norm=1                                 # run normal tests
run_valg=1                                 # run valgrind tests

VALGRIND="valgrind --leak-check=full --show-leak-kinds=all"
DIFF="diff -bBy"                            # -b ignore whitespace, -B ignore blank lines, -y do side-by-side comparison

mkdir -p test-data

INPUT=test-data/input.in                    # name for expected output file
EXPECT=test-data/expect.txt                 # name for expected output file
ACTUAL=test-data/actual.txt                 # name for actual output file
DIFFOUT=test-data/test.diff                 # name for diff output file
VALGOUT=test-data/valgrind.txt              # name for valgrind output file

function major_sep(){
    printf '%s\n' '====================================='
}
function minor_sep(){
    printf '%s\n' '-------------------------------------'
}

printf "Loading tests from $DATAFILE... "
source "$DATAFILE"
printf "%d tests loaded\n" "$T"

NTESTS=$T
VTESTS=$T
NPASS=0
NVALG=0

all_tests=$(seq $NTESTS)


# Check whether a single test is being run
single_test=$1
if ((single_test > 0 && single_test <= NTESTS)); then
    printf "Running single TEST %d\n" "$single_test"
    all_tests=$single_test
    NTESTS=1
    VTESTS=1
else
    printf "Running %d tests\n" "$NTESTS"
fi

# printf "tests: %s\n" "$all_tests"
printf "\n"

# Run normal tests: capture output and check against expected
if [ "$run_norm" = "1" ]; then
    printf "RUNNING NORMAL TESTS\n"
    for i in $all_tests; do
        rm -f $INPUT $EXPECT $ACTUAL $DIFFOUT $VALGOUT

        printf "TEST %2d %-18s : " "$i" "${tnames[i]}"
        FAIL="0"
        
        # Run the test

        # create the test C file
        c_file=$(printf "test-data/el_malloc_test_%02d.c" "$i")
        prog_file=$(printf "test-data/el_malloc_test_%02d"   "$i")
        printf "%s " "$c_file"
        printf "%s\n%s\n%s\n" "${defines[i]}" "${TEMPLATE}" "${cfile[i]}" > $c_file
        printf "Compiling "
        gcc -g -o $prog_file $c_file el_malloc.o -I .

        printf "Running : "
        $prog_file >& $ACTUAL
        # generate expected output
        printf "%s\n" "${output[i]}" > $EXPECT

        # Check for output differences, print side-by-side diff if problems
        if ! $DIFF $EXPECT $ACTUAL > $DIFFOUT
        then
            printf "FAIL: Output Incorrect\n"
            minor_sep
            printf "OUTPUT: EXPECT   vs   ACTUAL\n"
            cat $DIFFOUT
            if [ "$generate" == "1" ]; then
                printf "FULL ACTUAL\n"
                cat $ACTUAL
            fi
            minor_sep
            FAIL="1"
        fi

        if (( FAIL == 0 )); then
            printf "OK\n"
            ((NPASS++))
        fi            
    done
    printf "Finished:\n"
    printf "%2d / %2d Normal correct\n" "$NPASS" "$NTESTS"
    printf "\n"
fi

# ================================================================================

# Run valgrind tests: check only for problems identified by valgrind
if [ "$run_valg" = "1" ]; then
    printf "RUNNING VALGRIND TESTS\n"

    for i in $all_tests; do
        printf "TEST %2d %-18s : " "$i" "${tnames[i]}"
        
        # create the test C file
        c_file=$(printf "test-data/el_malloc_test_%02d.c" "$i")
        prog_file=$(printf "test-data/el_malloc_test_%02d"   "$i")
        printf "%s " "$c_file"
        printf "%s\n%s\n%s\n" "${defines[i]}" "${TEMPLATE}" "${cfile[i]}" > $c_file
        printf "Compiling "
        gcc -g -o $prog_file $c_file el_malloc.o -I .

        printf "Running : "
        # run code through valgrind
        $VALGRIND  $prog_file >& $VALGOUT

        # Check various outputs from valgrind
        if ! grep -q 'ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)' $VALGOUT ||
           ! grep -q 'in use at exit: 0 bytes in 0 blocks'  $VALGOUT ||
             grep -q 'definitely lost: 0 bytes in 0 blocks' $VALGOUT;
        then
            printf "FAIL: Valgrind detected problems\n"
            minor_sep
            cat $VALGOUT
            minor_sep
        else
            printf "Valgrind OK\n"
            ((NVALG++))
        fi
    done
    printf "Finished:\n"
    printf "%2d / %2d Valgrind correct\n" "$NVALG" "$VTESTS"
    printf "\n"
fi


major_sep
printf "OVERALL:\n"
printf "%2d / %2d Normal correct\n" "$NPASS" "$NTESTS"
printf "%2d / %2d Valgrind correct\n" "$NVALG" "$VTESTS"

