#!/bin/bash

make clean
make

echo "-----------------------------------------------"
echo " LOG OF EXECUTION $(date +"%x %r") "
echo "-----------------------------------------------"

./test_prios

echo "-----------------------------------------------"