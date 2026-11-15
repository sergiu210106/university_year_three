#!/bin/bash
# Build, verify, and benchmark in-place convolution in C++ and Java.
# Usage: scripts/run_tests.sh [runs] (default: 10)
# CASES="10 10 2; 7 13 2 3 16" bash scripts/run_tests.sh 2
set -euo pipefail
export LC_ALL=C

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"
INPUT="$BUILD/date.txt"
REF="$BUILD/output_seq.txt"
OUT="$BUILD/output.txt"
RUNS="${1:-10}"
CASES="${CASES:-10 10 2; 1000 1000 2 4 8 16; 10000 10000 2 4 8 16}"

if [[ ! "$RUNS" =~ ^[1-9][0-9]*$ ]]; then
    echo "runs must be a positive integer" >&2
    exit 1
fi

# Validate cases before building or replacing any existing results.
IFS=';' read -ra case_list <<< "$CASES"
for test_case in "${case_list[@]}"; do
    read -ra values <<< "$test_case"
    if [[ ${#values[@]} -lt 3 ]]; then
        echo "each case must contain: rows cols threads..." >&2
        exit 1
    fi
    for value in "${values[@]}"; do
        if [[ ! "$value" =~ ^[1-9][0-9]*$ ]]; then
            echo "case dimensions and thread counts must be positive integers" >&2
            exit 1
        fi
    done
done

bash "$ROOT/scripts/build.sh"
python3 "$ROOT/scripts/verify.py"

# Compare outputs: true only when both files are identical byte for byte.
# cmp streams the files, so the largest case needs no extra image-sized buffer.
same_content() {
    cmp -s "$1" "$2"
}

failures=0
executions=0

# run_program <language> <seq|par> <threads> <command...>
# Check EVERY measured output, including sequential executions, against the
# fixed C++ sequential reference. Comparison is outside the measured region.
run_program() {
    local language=$1 variant=$2 threads=$3
    shift 3
    local timings compute_ms total_ms status
    timings=$("$@")
    read -r compute_ms total_ms <<< "$timings"
    if same_content "$OUT" "$REF"; then
        status=OK
    else
        status=WRONG
        failures=$((failures + 1))
    fi
    executions=$((executions + 1))
    printf "%sx%s run=%s/%s %s %s p=%s compute=%s ms total=%s ms %s\n" \
        "$rows" "$cols" "$run" "$RUNS" "$language" "$variant" "$threads" \
        "$compute_ms" "$total_ms" "$status"
    printf "%s,%s,%s,%s,%s,%s,%s,%s\n" \
        "$language" "$variant" "$rows" "$cols" "$threads" "$run" \
        "$compute_ms" "$total_ms" >> "$ROOT/times.csv"
}

printf 'language,variant,rows,cols,threads,run,compute_ms,total_ms\n' > "$ROOT/times.csv"
for test_case in "${case_list[@]}"; do
    read -r rows cols thread_counts <<< "$test_case"
    seed=$((rows * 31 + cols * 17 + 3))
    echo "Generating ${rows}x${cols}, seed=$seed"
    "$BUILD/generate_input" "$rows" "$cols" "$INPUT" "$seed"
    # Unmeasured reference, generated once; all runs reload the same input file.
    "$BUILD/seq" "$INPUT" "$REF" > /dev/null
    for ((run = 1; run <= RUNS; run++)); do
        run_program cpp seq 1 "$BUILD/seq" "$INPUT" "$OUT"
        for p in $thread_counts; do
            run_program cpp par "$p" "$BUILD/par" "$INPUT" "$OUT" "$p"
        done
        run_program java seq 1 java -Xmx1g -cp "$BUILD" Seq "$INPUT" "$OUT"
        for p in $thread_counts; do
            run_program java par "$p" java -Xmx1g -cp "$BUILD" Par "$INPUT" "$OUT" "$p"
        done
    done
done

if [[ "$failures" -gt 0 ]]; then
    echo "FAILED: $failures of $executions outputs differ from the sequential reference" >&2
    exit 1
fi

# Average computation and total times, with speedup against the same-language
# sequential mean. Keep the existing CSV format for the assignment tables.
awk -F, 'NR > 1 {
    key = $1 "," $2 "," $3 "," $4 "," $5
    if (!(key in count)) order[++groups] = key
    compute[key] += $7
    total[key] += $8
    count[key]++
}
END {
    print "language,variant,rows,cols,threads,runs,avg_compute_ms,avg_total_ms,speedup"
    for (i = 1; i <= groups; i++) {
        key = order[i]
        split(key, fields, ",")
        baseline = fields[1] ",seq," fields[3] "," fields[4] ",1"
        mean = compute[key] / count[key]
        sequential = compute[baseline] / count[baseline]
        printf "%s,%d,%.12g,%.12g,%.12g\n", key, count[key], mean, total[key] / count[key], (mean > 0 ? sequential / mean : 0)
    }
}' "$ROOT/times.csv" > "$ROOT/averages.csv"

echo "PASS: all $executions measured outputs match the sequential reference."
echo "Results: $ROOT/times.csv and $ROOT/averages.csv"
