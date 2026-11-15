#!/bin/sh
set -e
cd "$(dirname "$0")"
mkdir -p build
mpicc main.c -o build/mpi_init
mpirun -np "${1:-2}" ./build/mpi_init
