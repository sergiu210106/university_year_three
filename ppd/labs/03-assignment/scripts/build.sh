#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MPICXX="${MPICXX:-mpic++}"
mkdir -p "$ROOT/build"

for program in seq mpi1 mpi11 mpi2 mpi3; do
    "$MPICXX" -std=c++11 -O2 -Wall -Wextra -pedantic \
        "$ROOT/cpp/$program.cpp" -o "$ROOT/build/$program"
done
for program in mpi1 mpi11; do
    "$MPICXX" -std=c++11 -O2 -Wall -Wextra -pedantic -DSYNCHRONOUS_SEND \
        "$ROOT/cpp/$program.cpp" -o "$ROOT/build/${program}_ssend"
done
"$MPICXX" -std=c++11 -O2 -Wall -Wextra -pedantic \
    "$ROOT/scripts/generate_input.cpp" -o "$ROOT/build/generate_input"

python3 - "$ROOT" "$MPICXX" <<'PY'
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(sys.argv[1])
compiler = shutil.which(sys.argv[2])
includes = subprocess.check_output([compiler, '--showme:incdirs'], text=True).split()
commands = []
sources = sorted((root / 'cpp').glob('*.cpp')) + [root / 'scripts/generate_input.cpp']
for source in sources:
    arguments = [compiler, '-std=c++11', '-O2', '-Wall', '-Wextra', '-pedantic']
    arguments.extend('-I' + directory for directory in includes)
    arguments.extend(['-c', str(source), '-o', str(root / 'build' / (source.stem + '.o'))])
    commands.append({'directory': str(root), 'file': str(source), 'arguments': arguments})
(root / 'build/compile_commands.json').write_text(json.dumps(commands, indent=2) + '\n')
PY
echo "Built sequential, MPI, synchronous-send, and generator executables in $ROOT/build"
