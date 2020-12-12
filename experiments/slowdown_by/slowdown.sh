#!/bin/bash

declare -i sum=0
declare -i time=0

for i in {1..10000}
do
	time=$( ./by ) 
	sum=$(($sum + $time))
done

declare -i average=0

echo "Total CPU time: " $sum
average=$(($sum / 10000))
echo "Average CPU time (100 thousand executions):" $average
