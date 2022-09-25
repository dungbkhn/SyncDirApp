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

find_same_files_in_list () {
	local dir1=$1
	local dir2=$2
	local count=0
	local curdir=$(pwd)	
	local rs code
	local pathname
	local filesize
	local md5hash
	local mtime
	local mtime_temp
	local listfiles="listfilesforcmp.txt"
	local listfilesize
	local outputfile_inremote="outputfile_inremote.txt"
	local loopforcount
	
	rm "$glb_memtemp_local"/"$listfiles"
	rm "$glb_memtemp_local"/"$outputfile_inremote"
	
	touch "$glb_memtemp_local"/"$listfiles"
	
	for pathname in "$dir1"/* ;do
		if [[ -f "$pathname" ]] ; then 
			pathname=$(basename "$pathname")
			md5hash=$(head -c 1024 "$dir1"/"$pathname" | md5sum | awk '{ print $1 }')
			#md5tailhash=$(get_src_content_file_md5sum "$pathname")
			mtime_temp=$(stat "$dir1"/"$pathname" --printf='%y\n')
			mtime=$(date +'%s' -d "$mtime_temp")
			filesize=$(stat -c %s "$dir1"/"$pathname")
			#printf "%s/%s/%s/%s/%s/%s\n" "./""$pathname" "f" "$filesize" "$md5hash" "$md5tailhash" "$mtime" 
			printf "%s/%s/%s/%s/%s\n" "./""$pathname" "f" "$filesize" "$md5hash" "$mtime" >> "$glb_memtemp_local"/"$listfiles"
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
	
	find_same_files_in_list "$param1""" "$param2"""
}


main
