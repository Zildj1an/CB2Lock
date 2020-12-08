#!/bin/bash

make clean
make

echo "-----------------------------------------------"
echo " LOG OF EXECUTION $(date +"%x %r") "
echo "-----------------------------------------------"

for i in {1..30}
do
	./test_prios >> log_20threads
	./test_prios -f >> log_20threads
done

echo "-----------------------------------------------"
