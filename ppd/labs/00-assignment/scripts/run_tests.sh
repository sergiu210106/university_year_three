#!/bin/bash
# Automated testing for the vector addition programs (C++ / Java, sequential / parallel).
#
# For every vector size n, RUNS runs are done. Each run:
#   1. generates a new random input file (a different seed for every run)
#   2. runs the sequential program; its output is the reference for this input
#   3. runs the parallel program for every thread count and checks that its output
#      file is identical to the sequential one
#   4. saves the times printed by every program (computation and whole program)
#
# Results:
#   times.csv    - every run:             impl,n,threads,run,compute_ms,total_ms
#   averages.csv - average per test case: impl,n,threads,runs,avg_compute_ms,avg_total_ms,
#                                         speedup_compute,speedup_total
#                  (speedup = sequential time / time, same language and n)
#
# usage: scripts/run_tests.sh [runs]
#   SIZES="10 1000000" THREADS="4 8" scripts/run_tests.sh 10

set -euo pipefail
export LC_ALL=C   # '.' as decimal separator in awk / printf

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
IN="$BUILD/in.txt"
REF="$BUILD/out_seq.txt"
OUT="$BUILD/out_par.txt"

RUNS="${1:-10}"
SIZES="${SIZES:-10 1000 100000 1000000}"
THREADS="${THREADS:-2 4 8 16}"

echo "Building..."
mkdir -p "$BUILD"
g++ -O2 -o "$BUILD/generate_input" "$ROOT/scripts/generate_input.cpp"
g++ -O2 -o "$BUILD/seq" "$ROOT/cpp/seq.cpp"
g++ -O2 -pthread -o "$BUILD/par" "$ROOT/cpp/par.cpp"
javac -d "$BUILD" "$ROOT/java/Seq.java" "$ROOT/java/Par.java"

failures=0

# same_content <file1> <file2>: true if the two files have exactly the same content
same_content() {
    cmp -s "$1" "$2"
}

# run_program <impl> <threads> <command...>
# runs the command and saves its times; a parallel program (impl *_par) must
# produce an output file identical to the sequential one
run_program() {
    local impl=$1 threads=$2
    shift 2

    local compute_ms total_ms status=REF
    read -r compute_ms total_ms <<< "$("$@")"
    if [[ $impl == *_par ]]; then
        if same_content "$OUT" "$REF"; then
            status=OK
        else
            status=WRONG
            failures=$((failures + 1))
        fi
    fi

    printf "n=%-8s run=%-2s %-8s threads=%-2s compute=%12s ms  total=%12s ms  %s\n" \
        "$n" "$run" "$impl" "$threads" "$compute_ms" "$total_ms" "$status"
    echo "$impl,$n,$threads,$run,$compute_ms,$total_ms" >> "$ROOT/times.csv"
}

echo "impl,n,threads,run,compute_ms,total_ms" > "$ROOT/times.csv"

for n in $SIZES; do
    for run in $(seq 1 "$RUNS"); do
        "$BUILD/generate_input" "$n" "$IN" "$((n + run))"

        run_program cpp_seq 1 "$BUILD/seq" "$IN" "$REF"
        for p in $THREADS; do
            run_program cpp_par "$p" "$BUILD/par" "$IN" "$OUT" "$p"
        done

        run_program java_seq 1 java -cp "$BUILD" Seq "$IN" "$REF"
        for p in $THREADS; do
            run_program java_par "$p" java -cp "$BUILD" Par "$IN" "$OUT" "$p"
        done
    done
done

# average times for every (impl, n, threads), in the order they first appear,
# and the speedup against the sequential program of the same language
awk -F, 'NR > 1 {
    key = $1 "," $2 "," $3
    if (!(key in count)) order[++k] = key
    compute[key] += $5
    total[key] += $6
    count[key]++
}
END {
    print "impl,n,threads,runs,avg_compute_ms,avg_total_ms,speedup_compute,speedup_total"
    for (i = 1; i <= k; i++) {
        key = order[i]
        split(key, f, ",")
        lang = substr(f[1], 1, index(f[1], "_") - 1)
        seq = lang "_seq," f[2] ",1"
        c = compute[key] / count[key]
        t = total[key] / count[key]
        sc = compute[seq] / count[seq]
        st = total[seq] / count[seq]
        printf "%s,%d,%.6f,%.6f,%.3f,%.3f\n", key, count[key], c, t, (c > 0 ? sc / c : 0), st / t
    }
}' "$ROOT/times.csv" > "$ROOT/averages.csv"

echo
column -s, -t < "$ROOT/averages.csv"
echo

if [ "$failures" -gt 0 ]; then
    echo "FAILED: $failures parallel result(s) different from the sequential result"
    exit 1
fi
echo "All parallel results are identical to the sequential results."
