#!/bin/bash
# Testing of the microbenchmark 

# Function to disable hyperthreading 
function disable_hyp {
	
	# NOTE: Function written by someone else:
	#       https://askubuntu.com/questions/942728/disable-hyper-threading-in-ubuntu/942843#942843

	echo 0 > /sys/devices/system/cpu/cpu1/online
	echo 0 > /sys/devices/system/cpu/cpu3/online
	echo 0 > /sys/devices/system/cpu/cpu5/online
	echo 0 > /sys/devices/system/cpu/cpu7/online
	grep "" /sys/devices/system/cpu/cpu*/topology/core_id

	grep -q '^flags.*[[:space:]]ht[[:space:]]' /proc/cpuinfo && \
    		echo "Hyper-threading is supported"

	grep -E 'model|stepping' /proc/cpuinfo | sort -u

	# https://packages.debian.org/stretch/amd64/stress/download
	stress --cpu 8 --io 1 --vm 1 --vm-bytes 128M --timeout 10s
}

# sudo apt install cowsay figlet toilet
cowsay -f tux 'CB2Lock microbenchmark '$(date +"%x %r") | toilet --metal -f term 

# Disable hyperthreading
#disable_hyp &> /dev/null

if [ $# -eq 1 ]; then
	if test -f $1; then
   		echo "Parsing configuration file..."
		source $1
	fi
fi
 
cd src

for lock in {1..3}
do
	for m in {1..100}
	do
		for iterations in {1..100}
		do
			if [[ $iterations -lt $LOW_ITER ]]; then
				continue
			fi

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
				./test_prios -n $i -p $lock -i $iterations
	
				if [[ $i -eq $HIGH_THREAD ]]; then
					break
				fi
			done

			if [[ $iterations -eq $HIGH_ITER ]]; then
				break
			fi
		done

		if [[ $m -eq $TIMES ]]; then
			break
		fi

		echo "-----------------------------------------------"
	done
done
