cd ../../examples
make
cd ../threads
make
cd ../vm/
make
cd build

../../utils/pintos -f -q
../../utils/pintos -p ../../examples/bubsort -a bubsort -- -q
../../utils/pintos -q run 'bubsort'
