#!/bin/bash

shopt -s dotglob
shopt -s nullglob

memtemp=/var/res/backup/.Temp

#for COPY
copyfilesize="10MB"
truncsize=10000000

SECONDS=0
#ten file chua ds file tu phia local
param1=$1
#thu muc dang sync phia remote
param2=$(echo "$2" | tr -d '\n' | xxd -r -p)
#outputfilename 
param3=$3

# declare array
declare -a names
declare -a isfile
declare -a filesize
declare -a md5hash
declare -a mtime
declare -a hassamefile

declare -a names_remote
declare -a isfile_remote
declare -a filesize_remote
declare -a md5hash_remote
declare -a mtime_remote
declare -a hassamefile_remote

declare -a names_nt
declare -a filesize_nt
declare -a md5hash_nt
declare -a mtime_nt

declare -a names_remote_nt
declare -a filesize_remote_nt
declare -a md5hash_remote_nt
declare -a mtime_remote_nt
declare -a isselected_remote_nt

found=0
count=0
mtime_temp=""

echo "$param2"
echo "$memtemp""/""$param1"

if [[ -f "$memtemp""/""$param1" ]] ; then

	while IFS=/ read -r beforeslash afterslash_1 afterslash_2 afterslash_3 afterslash_4 afterslash_5
	do
		names[$count]="$afterslash_1"
		isfile[$count]="$afterslash_2"
		filesize[$count]="$afterslash_3"
		md5hash[$count]="$afterslash_4"
		mtime[$count]="$afterslash_5"
		hassamefile[$count]=0
		count=$(($count + 1))
	done < "$memtemp""/""$param1"

	#len=${#names[@]}
	rm "$memtemp"/"$param3"
	touch "$memtemp"/"$param3"
	
	
	count=0

	for pathname in "$param2"/*; do
		
		if [[ ! -d "$pathname" ]] ; then 
			names_remote[$count]=$(basename "$pathname")
			hassamefile_remote[$count]=0
			isfile_remote[$count]="f"
			filesize_remote[$count]=$(stat -c %s "$pathname")
			md5hash_remote[$count]=$(head -c 8192 "$pathname" | md5sum | awk '{ print $1 }')
			#mtime_temp=$(stat "$pathname" --printf='%y\n')
			mtime_remote[$count]=$(stat "$pathname" -c %Y)
			#mtime_remote[$count]=$(date +'%s' -d "$mtime_temp")
			#echo "%s/%s/%s/%s/%s\n" "$pathname" "${names_remote[$count]}" "${filesize_remote[$count]}"
			
			count=$(($count + 1))
		fi

	done
	
	#1:same file
	#0:can hash tung phan de kiem tra ---> append phai hash
	#5:chi ton tai phia local do phia remote kich thuoc qua lon nen bi xoa, hoac khac noi dung --->can copy
	count=0
	for i in "${!names[@]}" ; do

		for j in "${!names_remote[@]}" ; do
			#echo "herererererererererer-----""${names[$i]}""-------""${names_remote[$j]}"
			if [[ "${isfile[$i]}" == "f" ]] && [[ "${names[$i]}" == "${names_remote[$j]}" ]] ; then
					
					hassamefile[$i]=1
					hassamefile_remote[$j]=1
					if [[ "${mtime[$i]}" == "${mtime_remote[$j]}" ]] && [[ ${filesize[$i]} -eq ${filesize_remote[$j]} ]] && [[ "${md5hash[$i]}" == "${md5hash_remote[$j]}" ]] ; then
						printf "./%s/1/%s/0/null/0/0\n" "${names[$i]}" "${names_remote[$j]}" >> "$memtemp""/""$param3"
					elif [[ "${md5hash[$i]}" != "${md5hash_remote[$j]}" ]] ; then
						rm "$param2""/""${names_remote[$j]}"
						printf "./%s/5/null/0/null/0/0\n" "${names[$i]}" >> "$memtemp""/""$param3"
					elif [[ ${filesize[$i]} -lt ${filesize_remote[$j]} ]] ; then
						rm "$param2""/""${names_remote[$j]}"
						printf "./%s/5/null/0/null/0/0\n" "${names[$i]}" >> "$memtemp""/""$param3"					
					else
						printf "./%s/0/%s/%s/%s/%s/%s\n" "${names[$i]}" "${names_remote[$j]}" "${filesize_remote[$j]}" "${md5hash_remote[$j]}" "${mtime_remote[$j]}" "${mtime[$i]}" >> "$memtemp""/""$param3"
					fi
					count=1

					break
			fi
		done

		#if [ "$count" -eq 1 ] ; then
		#	echo "found"
		#fi
		
	done


	#echo '------------------------file remains in local side------------------------------' >> "$memtemp""/""$param3"

	count=0
	for i in "${!hassamefile[@]}"
	do
		if [[ ${hassamefile[$i]} -eq 0 ]] && [[ "${isfile[$i]}" == "f" ]] ; then
			names_nt[$count]="${names[$i]}"
			filesize_nt[$count]="${filesize[$i]}"
			md5hash_nt[$count]="${md5hash[$i]}"
			mtime_nt[$count]="${mtime[$i]}"
			count=$(($count + 1))
		fi
		
	done


	#echo "$count"'------------------------file remains in remote side------------------------------' >> "$memtemp""/""$param3"

	count=0
	for i in "${!hassamefile_remote[@]}"
	do
		if [[ ${hassamefile_remote[$i]} -eq 0 ]] && [[ "${isfile_remote[$i]}" == "f" ]] ; then
			names_remote_nt[$count]="${names_remote[$i]}"
			isselected_remote_nt[$count]=0
			filesize_remote_nt[$count]="${filesize_remote[$i]}"
			md5hash_remote_nt[$count]="${md5hash_remote[$i]}"
			mtime_remote_nt[$count]="${mtime_remote[$i]}"
			count=$(($count + 1))
		fi
		
	done

	#echo "$count"'------------------------file so sanh------------------------------' >> "$memtemp""/""$param3"
	#so sanh
	#2:da co san o remote, sai ten thoi--->doi ten
	#4:file chi co o local, ko co o remote ---> can copy
	for i in "${!names_nt[@]}"
	do
		count=0
		for j in "${!names_remote_nt[@]}"
		do
			if [[ ${isselected_remote_nt[$j]} -eq 0 ]] && [[ "${mtime_nt[$i]}" == "${mtime_remote_nt[$j]}" ]] && [[ "${filesize_nt[$i]}" == "${filesize_remote_nt[$j]}" ]] && [[ "${md5hash_nt[$i]}" == "${md5hash_remote_nt[$j]}" ]] ; then
				printf "./%s/2/%s/0/null/0/0\n" "${names_nt[$i]}" "${names_remote_nt[$j]}" >> "$memtemp""/""$param3"
				mv "$param2""/""${names_remote_nt[$j]}" "$param2""/""${names_nt[$i]}"
				isselected_remote_nt[$j]=1
				count=1
				break
			fi
		done
		
		if [[ $count -eq 0 ]] ; then
			#echo 'do some thing'
			printf "./%s/4/null/0/null/0/0\n" "${names_nt[$i]}" >> "$memtemp""/""$param3"
		fi
		
	done

	#echo '------------------------file bi xoa------------------------------' >> "$memtemp""/""$param3"
	#xoa nhung file con lai
	#3:file ko co o local nen bi xoa o remote
	for i in "${!names_remote_nt[@]}"
	do
		if [[ ${isselected_remote_nt[$i]} -eq 0 ]] ; then
			rm "$param2""/""${names_remote_nt[$i]}"
			printf "./%s/3/%s/0/null/0/0\n" "${names_remote_nt[$i]}" "${names_remote_nt[$i]}" >> "$memtemp""/""$param3"
		fi
	done
	echo "elapsed time (using \$SECONDS): $SECONDS seconds" > "$memtemp""/""thoigian.txt"
	rm "$memtemp""/""$param1"
	echo './' >> "$memtemp""/""$param3"
fi
