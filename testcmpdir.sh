#!/bin/bash
      
shopt -s dotglob
shopt -s nullglob

if [[ -f "./config/syncfolder" ]] ; then
	glb_mainmem_local=$(head -n 1 "./config/syncfolder")
	echo "Dir will sync:""$glb_mainmem_local"
else
	echo "Error reading config file"
	exit 1
fi

glb_root_local=$(pwd)
echo "$glb_root_local"

glb_root_remote=/var/res/backup

glb_mainmem_remote="$glb_root_remote"/SyncDir
glb_memtemp_local="$glb_root_local"/.temp
glb_memtemp_remote="$glb_root_remote"/.Temp

find_same_dirs_in_list () {
	local dir1=$1
	local dir2=$2
	local curdir=$(pwd)
	local rs code
	local count
	local pathname
	local subpathname
	local filesize
	local listfiles="listdirsforcmp.txt"
	local listfilesize
	local outputdir_inremote="outputdir_inremote.txt"
	
	rm "$glb_memtemp_local"/"$listfiles"
	rm "$glb_memtemp_local"/"$outputdir_inremote"

	touch "$glb_memtemp_local"/"$listfiles"
	
	for pathname in "$dir1"/* ; do
		if [[ -d "$pathname" ]] ; then 
			pathname=$(basename "$pathname")
			printf "%s/b/%s/%s\n" "./""$pathname" "d" "0" >> "$glb_memtemp_local"/"$listfiles"
			count=0

			for subpathname in "$dir1"/"$pathname"/* ; do
				if [[ -d "$subpathname" ]] ; then 
					subpathname=$(basename "$subpathname")
					printf "%s/n/%s/%s\n" "./""$subpathname" "d" "1" >> "$glb_memtemp_local"/"$listfiles"
				else
					subpathname=$(basename "$subpathname")
					printf "%s/n/%s/%s\n" "./""$subpathname" "f" "1" >> "$glb_memtemp_local"/"$listfiles"
				fi
				count=$(($count + 1))
				if [[ "$count" == 5 ]] ; then
					break
				fi
			done
			printf "%s/e/%s/%s\n" "./""$pathname" "d" "0" >> "$glb_memtemp_local"/"$listfiles"		
		fi
		
	done	
	
	listfilesize=$(stat -c %s "$glb_memtemp_local"/"$listfiles")

	if [[ $listfilesize -eq 0 ]] ; then 
		return 0
	fi

	
	pathname=$(echo "/home/dungnt/TestDir" | tr -d '\n' | xxd -pu -c 1000000)

	echo "$pathname"
}

main(){
	local param1="$glb_mainmem_local"
	local param2="$glb_mainmem_remote"
	
	find_same_dirs_in_list "$param1""" "$param2"""
}


main
