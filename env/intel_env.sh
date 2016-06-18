#!/bin/bash
# source this script to setup compiler ENV

export BUILD=intel
export COMPILER="intel"

echo "Using Intel icc, icpc, and ifort compilers for all binary builds"

source  /opt/intel/bin/compilervars.sh -arch intel64 -platform linux 

export CC="icc"
export CXX="icpc"
export CPP="${CC} -E"
export F90="ifort"
export F77="ifort"
export FC="ifort"
export CFLAGS="-v -Wall -O2"
export CXXFLAGS="-v -Wall -O2"
export FFLAGS="-v -warn all -O2 -mp1 -assume buffered_io"
export FCFLAGS="-v -warn all -O2 -mp1 -assume buffered_io"
export FFLAGS90="-v -warn all -O2 -mp1 -assume buffered_io"
export NUMCPUS=$(cat /proc/cpuinfo | grep processor | wc -l | tr -d " ")
export OMP_NUM_THREADS=${NUMCPUS}
export OMP_PROC_BIND="true"
export FORT_BUFFERED="true"
export KMP_AFFINITY="granularity=fine,compact,1,0,verbose"


