#!/bin/bash

PROGRAM=./my-sum
ITERATIONS=100
MAX_N=200
MAX_M=16

echo "Starting stress test..."

for ((t=1; t<=ITERATIONS; t++))
do
    # Random size
    n=$((RANDOM % MAX_N + 1))

    # Random m such that 1 <= m <= n and m <= MAX_M
    max_allowed_m=$(( n < MAX_M ? n : MAX_M ))
    m=$((RANDOM % max_allowed_m + 1))

    # Generate random input
    rm -f input.dat output.dat expected.dat
    for ((i=0; i<n; i++))
    do
        echo $((RANDOM % 50 - 25)) >> input.dat
    done

    # Compute correct prefix sum (sequential oracle)
    awk '
    {
        sum += $1;
        printf "%d ", sum;
    }
    END { printf "\n"; }
    ' input.dat > expected.dat

    # Run your program
    $PROGRAM $n $m input.dat output.dat > /dev/null 2>&1

    # Compare outputs
    if ! diff -q expected.dat output.dat > /dev/null; then
        echo "❌ MISMATCH DETECTED"
        echo "n=$n m=$m"
        echo "Input:"
        cat input.dat
        echo "Expected:"
        cat expected.dat
        echo "Got:"
        cat output.dat
        exit 1
    fi

    echo "✔ Test $t passed (n=$n m=$m)"
done

echo "All $ITERATIONS tests passed successfully!"