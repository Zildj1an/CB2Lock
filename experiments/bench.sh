#!/bin/bash

echo "-----------------------------------------------"
echo " LOG OF EXECUTION $(date +"%x %r") "
echo "-----------------------------------------------"

for i in {2..30}
do
	if [[ $i -lt $LOW_THREAD ]]; then
		continue
	fi
	
	./test_prios -n $i
	./test_prios -n $i -f
	
	if [[ $i -eq $HIGH_THREAD ]]; then
		break
	fi

done

echo "-----------------------------------------------"
