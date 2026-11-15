#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MPIEXEC="${MPIEXEC:-mpirun}"
TIMEOUT="${TIMEOUT:-timeout}"
LIMIT="${TIMEOUT_SECONDS:-30}"
"$ROOT/scripts/build.sh"
mkdir -p "$ROOT/build/inputs" "$ROOT/build/outputs"

cat > "$ROOT/build/inputs/supplied_1.txt" <<'EOF'
16
9 9 9 9 4 4 4 4 4 4 4 4 9 9 9 9
EOF
cat > "$ROOT/build/inputs/supplied_2.txt" <<'EOF'
16
9 9 9 9 5 5 5 5 5 5 5 5 9 9 9 9
EOF

for spec in 'equal1000 1000 1000 3101 3102' 'equal10000 10000 10000 3201 3202' 'unequal 100 100000 3301 3302'; do
    read -r name n1 n2 seed1 seed2 <<< "$spec"
    "$ROOT/build/generate_input" "$n1" "$ROOT/build/inputs/${name}_1.txt" "$seed1"
    "$ROOT/build/generate_input" "$n2" "$ROOT/build/inputs/${name}_2.txt" "$seed2"
done

header='case,n1,n2,variant,processes,total_ms,speedup'
echo "$header" > "$ROOT/times.csv"
echo "$header" > "$ROOT/synchronous_times.csv"
comparisons=0

record_parallel() {
    local variant="$1" processes="$2" mode="$3" csv="$4"
    local program="$variant" output elapsed speedup
    if [[ "$mode" == synchronous ]]; then program="${variant}_ssend"; fi
    output="$ROOT/build/outputs/${name}_${program}_${processes}.txt"
    elapsed="$("$TIMEOUT" "$LIMIT" "$MPIEXEC" --oversubscribe -np "$processes" \
        "$ROOT/build/$program" "$input1" "$input2" "$output")"
    cmp "$reference" "$output"
    speedup="$(awk -v seq="$baseline" -v par="$elapsed" 'BEGIN {printf "%.6f", seq / par}')"
    printf '%s,%s,%s,%s,%s,%s,%s\n' "$name" "$n1" "$n2" "$variant" "$processes" "$elapsed" "$speedup" >> "$csv"
    comparisons=$((comparisons + 1))
    printf 'PASS %-10s %-12s p=%-2s %s ms\n' "$name" "$program" "$processes" "$elapsed"
}

for spec in 'supplied 16 16' 'equal1000 1000 1000' 'equal10000 10000 10000' 'unequal 100 100000'; do
    read -r name n1 n2 <<< "$spec"
    input1="$ROOT/build/inputs/${name}_1.txt"
    input2="$ROOT/build/inputs/${name}_2.txt"
    reference="$ROOT/build/outputs/${name}_seq.txt"
    baseline="$("$TIMEOUT" "$LIMIT" "$ROOT/build/seq" "$input1" "$input2" "$reference")"
    printf '%s,%s,%s,seq,1,%s,1.000000\n' "$name" "$n1" "$n2" "$baseline" >> "$ROOT/times.csv"
    if [[ "$name" != equal1000 ]]; then
        worker_processes='5 9 17'
        if [[ "$name" == supplied ]]; then worker_processes=5; fi
        for processes in $worker_processes; do
            for variant in mpi1 mpi11 mpi3; do
                record_parallel "$variant" "$processes" standard "$ROOT/times.csv"
            done
            for variant in mpi1 mpi11; do
                record_parallel "$variant" "$processes" synchronous "$ROOT/synchronous_times.csv"
            done
        done
    fi
    if [[ "$name" != equal10000 ]]; then
        collective_processes='4 8 16'
        if [[ "$name" == supplied ]]; then collective_processes=4; fi
        for processes in $collective_processes; do
            record_parallel mpi2 "$processes" standard "$ROOT/times.csv"
        done
    fi
done
echo "PASS: all $comparisons parallel outputs match the sequential output byte for byte."
echo "Results: $ROOT/times.csv and $ROOT/synchronous_times.csv"
