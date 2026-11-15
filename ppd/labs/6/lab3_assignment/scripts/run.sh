#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [[ $# -lt 1 ]]; then
    echo "usage: $0 <seq|mpi1|mpi11|mpi2|mpi3> [processes] [input1 [input2 [output]]]" >&2
    exit 1
fi
variant="$1"
shift
case "$variant" in
    seq) exec "$ROOT/build/seq" "$@" ;;
    mpi1|mpi11|mpi2|mpi3)
        processes="${1:-5}"
        if [[ $# -gt 0 ]]; then shift; fi
        exec "${MPIEXEC:-mpirun}" --oversubscribe -np "$processes" "$ROOT/build/$variant" "$@"
        ;;
    *) echo "unknown variant: $variant" >&2; exit 1 ;;
esac
