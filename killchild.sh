#!/bin/bash

shopt -s dotglob
shopt -s nullglob
loop=0
while true; do
	loop=$(( $loop + 1 ))
	if [[ $loop -eq 3 ]] ; then
		break
	else
		sleep 0.1
	fi
	
	while true; do

		num=$(ps -aux | grep 'sshsyncdir_v2.sh' | wc -l)

		echo $num

		if [ $num -gt 1 ] ; then
			rs=$(ps -aux | grep 'sshsyncdir_v2.sh' | head -n $num | awk '{ print $2 }')

			echo "will kill ""$rs"
		
			kill $rs
			
			sleep 0.1
		else
			break
		fi
	done

	while true; do
		num=$(ps -aux | grep 'client_by_tox_protocol' | wc -l)

		echo $num

		if [ $num -gt 1 ] ; then
			rs=$(ps -aux | grep 'client_by_tox_protocol' | head -n $num | awk '{ print $2 }')

			echo "will kill ""$rs"
		
			kill $rs

			sleep 0.1
		else
			break
		fi
	done
done
