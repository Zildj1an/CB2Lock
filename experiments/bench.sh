#!/bin/bash

echo "-----------------------------------------------"
echo " LOG OF EXECUTION $(date +"%x %r") "
echo "-----------------------------------------------"

for m in {1..100}
do
	if [[ $m -eq $ITERATIONS ]]; then
		break
	fi

	for i in {3..6}
	do
		# Clean everything
		make clean &> /dev/null
		make &> /dev/null
		hash -r
		sync
		echo 3 > /proc/sys/vm/drop_caches 

		# Run
		./test_prios -n $i
		./test_prios -n $i -f
	done

	echo "-----------------------------------------------"
done
