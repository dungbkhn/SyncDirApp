#!/bin/bash

shopt -s dotglob
shopt -s nullglob

while true; do

	rs=$(ps -aux | grep 'sshsyncdir_v2.sh' | wc -l)

	echo $rs

	if [ $rs -gt 1 ] ; then
		rs=$(ps -aux | grep 'sshsyncdir_v2.sh' | head -n 1 | awk '{ print $2 }')

		echo "will kill ""$rs"
	
		kill "$rs"
	else
		break
	fi
done

while true; do
	rs=$(ps -aux | grep 'client_by_tox_protocol' | wc -l)

	echo $rs

	if [ $rs -gt 1 ] ; then
		rs=$(ps -aux | grep 'client_by_tox_protocol' | head -n 1 | awk '{ print $2 }')

		echo "will kill ""$rs"
	
		kill "$rs"

	else
		break
	fi
done
