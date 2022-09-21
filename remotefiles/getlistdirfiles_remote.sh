#!/bin/bash

shopt -s dotglob
shopt -s nullglob

dir=$(echo "$1" | tr -d '\n' | xxd -r -p)
outputfile="$2"

declare -a name
declare -a type
declare -a headhash
declare -a mtime
declare -a size


count=0
for pathname in "$dir"/* ; do
	if [ -d "$pathname" ] ; then 
		name[$count]=$(basename "$pathname")
		type[$count]="d"
		mtime[$count]=$(stat "$pathname" -c '%Y')
		size[$count]=0
		headhash[$count]="null"
	else
		name[$count]=$(basename "$pathname")
		type[$count]="f"
		headhash[$count]=$(head -c 1024 "$pathname" | md5sum | awk '{ print $1 }')
		size[$count]=$(stat -c %s "$pathname")
		mtime[$count]=$(stat "$pathname" -c '%Y')
	fi
	count=$(($count + 1))
done

truncate -s 0 "$outputfile"

for i in "${!name[@]}" ; do
	printf "./%s/%s/%s/%s/%s\n" "${name[$i]}" "${type[$i]}" "${mtime[$i]}" "${size[$i]}" "${headhash[$i]}" >> "$outputfile"
done
