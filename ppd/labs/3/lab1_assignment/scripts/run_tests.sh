#!/bin/bash
# Automated testing for the convolution programs
# (C++ static / dynamic allocation, Java; sequential / parallel with 5 distribution variants).
#
# For every test case (n, m, k, thread counts):
#   1. the input files MatrixF.txt and ConvolutionM.txt are generated once (fixed seed);
#      all RUNS runs of the test case use the same files
#   2. every run executes the C++ static sequential program first; its output is the
#      reference. Then every other program (C++ dynamic sequential, Java sequential, and
#      every parallel variant / thread count in both languages) is executed, and its output
#      file must be identical to the reference
#   3. the times printed by every program (computation and whole program) are saved
#
# Results:
#   times.csv    - every execution: lang,alloc,variant,n,m,k,threads,run,compute_ms,total_ms
#   averages.csv - average per test case: lang,alloc,variant,n,m,k,threads,runs,
#                  avg_compute_ms,avg_total_ms,speedup_compute,speedup_total
#                  (speedup = time of the sequential program of the same language and
#                   allocation / time, same n, m, k)
#
# usage: scripts/run_tests.sh [runs]
#   CASES: test cases separated by ';', each "n m k threads..."
#   VARIANTS: distribution variants of the parallel programs
#   CASES="10 10 3 4; 7 13 3 3 16" VARIANTS="horizontal cyclic" scripts/run_tests.sh 2

set -euo pipefail
export LC_ALL=C   # '.' as decimal separator in awk / printf

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
F_IN="$BUILD/MatrixF.txt"
C_IN="$BUILD/ConvolutionM.txt"
REF="$BUILD/output_seq.txt"
OUT="$BUILD/output.txt"

RUNS="${1:-10}"
CASES="${CASES:-10 10 3 4; 1000 1000 3 2 4 8 16; 10 10000 5 2 4 8 16; 10000 10 5 2 4 8 16; 10000 10000 5 2 4 8 16}"
VARIANTS="${VARIANTS:-horizontal vertical block linear cyclic}"

echo "Building..."
mkdir -p "$BUILD"
g++ -O2 -o "$BUILD/generate_input" "$ROOT/scripts/generate_input.cpp"
g++ -O2 -DSTATIC_ALLOC -o "$BUILD/seq_static" "$ROOT/cpp/seq.cpp"
g++ -O2 -o "$BUILD/seq_dynamic" "$ROOT/cpp/seq.cpp"
g++ -O2 -pthread -DSTATIC_ALLOC -o "$BUILD/par_static" "$ROOT/cpp/par.cpp"
g++ -O2 -pthread -o "$BUILD/par_dynamic" "$ROOT/cpp/par.cpp"
javac -d "$BUILD" "$ROOT"/java/*.java

failures=0

# same_content <file1> <file2>: true if the two files have exactly the same content
same_content() {
    cmp -s "$1" "$2"
}

# run_program <lang> <alloc> <variant> <threads> <command...>
# runs the command and saves its times. The C++ static sequential program writes the
# reference output; every other program must produce an output identical to it.
run_program() {
    local lang=$1 alloc=$2 variant=$3 threads=$4
    shift 4

    local compute_ms total_ms status=REF
    read -r compute_ms total_ms <<< "$("$@")"
    if [[ ! ($lang == cpp && $alloc == static && $variant == seq) ]]; then
        if same_content "$OUT" "$REF"; then
            status=OK
        else
            status=WRONG
            failures=$((failures + 1))
        fi
    fi

    printf "%-11s run=%-2s %-4s %-7s %-10s threads=%-2s compute=%12s ms  total=%12s ms  %s\n" \
        "${n}x${m},k=$k" "$run" "$lang" "$alloc" "$variant" "$threads" "$compute_ms" "$total_ms" "$status"
    echo "$lang,$alloc,$variant,$n,$m,$k,$threads,$run,$compute_ms,$total_ms" >> "$ROOT/times.csv"
}

echo "lang,alloc,variant,n,m,k,threads,run,compute_ms,total_ms" > "$ROOT/times.csv"

IFS=';' read -ra case_list <<< "$CASES"
for test_case in "${case_list[@]}"; do
    read -r n m k threads <<< "$test_case"
    "$BUILD/generate_input" "$n" "$m" "$k" "$F_IN" "$C_IN" "$((n * 31 + m * 17 + k))"

    for run in $(seq 1 "$RUNS"); do
        run_program cpp static seq 1 "$BUILD/seq_static" "$F_IN" "$C_IN" "$REF"
        for alloc in static dynamic; do
            if [ "$alloc" = dynamic ]; then
                run_program cpp dynamic seq 1 "$BUILD/seq_dynamic" "$F_IN" "$C_IN" "$OUT"
            fi
            for variant in $VARIANTS; do
                for p in $threads; do
                    run_program cpp "$alloc" "$variant" "$p" "$BUILD/par_$alloc" "$F_IN" "$C_IN" "$OUT" "$p" "$variant"
                done
            done
        done

        run_program java - seq 1 java -cp "$BUILD" Seq "$F_IN" "$C_IN" "$OUT"
        for variant in $VARIANTS; do
            for p in $threads; do
                run_program java - "$variant" "$p" java -cp "$BUILD" Par "$F_IN" "$C_IN" "$OUT" "$p" "$variant"
            done
        done
    done
done

# average times for every (lang, alloc, variant, n, m, k, threads), in the order they first
# appear, and the speedup against the sequential program of the same language and allocation
awk -F, 'NR > 1 {
    key = $1 "," $2 "," $3 "," $4 "," $5 "," $6 "," $7
    if (!(key in count)) order[++q] = key
    compute[key] += $9
    total[key] += $10
    count[key]++
}
END {
    print "lang,alloc,variant,n,m,k,threads,runs,avg_compute_ms,avg_total_ms,speedup_compute,speedup_total"
    for (i = 1; i <= q; i++) {
        key = order[i]
        split(key, f, ",")
        seq = f[1] "," f[2] ",seq," f[4] "," f[5] "," f[6] ",1"
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
    echo "FAILED: $failures result(s) different from the sequential result"
    exit 1
fi
echo "All results are identical to the sequential result."
