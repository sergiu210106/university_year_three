#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/build"
CXX="${CXX:-c++}"
"$CXX" -std=c++20 -O2 -Wall -Wextra -pedantic "$ROOT/cpp/seq.cpp" -o "$ROOT/build/seq"
"$CXX" -std=c++20 -O2 -Wall -Wextra -pedantic -pthread "$ROOT/cpp/par.cpp" -o "$ROOT/build/par"
"$CXX" -std=c++20 -O2 -Wall -Wextra -pedantic "$ROOT/scripts/generate_input.cpp" -o "$ROOT/build/generate_input"
javac -Xlint:all -d "$ROOT/build" "$ROOT"/java/*.java
