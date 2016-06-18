#!/bin/bash
# source this script to setup compiler ENV

export BUILD="gnu"
export COMPILER="gnu"

echo "Using GCC, G++, and GFORTRAN compilers for all binary builds"

export CC="gcc"
export CXX="g++"
export CPP="${CC} -E"
export F90="gfortran"
export F77="gfortran"
export FC="gfortran"
export CFLAGS="-O2 -Wall -v"
export CXXFLAGS="-O2 -Wall -v"
export FFLAGS="-O2 -Wall -v"
export FCFLAGS="-O2 -Wall -v"
export FFLAGS90="-O2 -Wall -v"
export NUMCPUS=$(cat /proc/cpuinfo | grep processor | wc -l | tr -d " ")
export OMP_NUM_THREADS=${NUMCPUS}
export OMP_PROC_BIND="true"
export FORT_BUFFERED="true"
export KMP_AFFINITY="granularity=fine,compact,1,0,verbose"
