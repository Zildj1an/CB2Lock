#!/bin/bash
# Testing of the microbenchmark 

echo "-----------------------------------------------"
echo " LOG OF EXECUTION $(date +"%x %r") "
echo "-----------------------------------------------"

# Disable hyperthreading
./hyperthreading.sh &> /dev/null

for m in {1..100}
do
	for i in {1..100}
	do
		if [[ $i -lt $LOW_THREAD ]]; then
			continue
		fi

		# Clean everything
		make clean &> /dev/null
		make &> /dev/null
		hash -r
		sync
		echo 3 > /proc/sys/vm/drop_caches 

		# Run
		./test_prios -n $i
		./test_prios -n $i -f
	
		if [[ $i -eq $HIGH_THREAD ]]; then
			break
		fi
	done

	if [[ $m -eq $ITERATIONS ]]; then
		break
	fi

	echo "-----------------------------------------------"
done
