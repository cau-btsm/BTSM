cd ../../examples
make
cd ../threads
make
cd ../vm/
make
cd build

../../utils/pintos -f -q
../../utils/pintos -p ../../examples/matmult -a matmult -- -q
../../utils/pintos -q run 'matmult'
