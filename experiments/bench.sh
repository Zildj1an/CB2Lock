#!/bin/bash

echo "-----------------------------------------------"
echo " LOG OF EXECUTION $(date +"%x %r") "
echo "-----------------------------------------------"

for i in {1..30}
do
	./test_prios -n $THREAD_COUNT
	./test_prios -n $THREAD_COUNT -f
done

echo "-----------------------------------------------"
