#!/bin/bash

#Windows forbidden character of name:  <  >  :  "  /  \  |  ?  * 
        
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
glb_clienttox_local="$glb_root_local"/clienttox_replacessh

glb_stopmerge="$glb_memtemp_local"/stopmerge
glb_getlistdirfiles_remote=getlistdirfiles_remote.sh
glb_compare_listdir_inremote=comparelistdir_remote.sh
glb_compare_listfile_inremote=comparelistfile_remote.sh

mainlogfile="$glb_memtemp_local"/mainlog.txt
errorlogfile="$glb_memtemp_local"/errorlog.txt
hashlogfile="$glb_memtemp_local"/hashlog.txt
testhashlogfile="$glb_memtemp_local"/testhashlog.txt

glb_befDirHash=""
glb_afDirHash=""
glb_hashcount=0
glb_hashcountmodulo=0

glb_endclienttox_pid=0
glb_prt=2

#----------------------------------------TOOLS-------------------------------------

mech(){
	local param=$1
	
	if [[ $glb_prt -eq 1 ]] ; then
		echo "$param"
	elif [[ $glb_prt -eq 2 ]] ; then
		echo "$param" >> "$mainlogfile"
	elif [[ $glb_prt -eq 3 ]] ; then
		echo "$param"
		echo "$param" >> "$mainlogfile"
	fi
}


run_command_in_remote(){
	local path="$glb_memtemp_local"
	local cmdfile=$path"/cmd.txt"
	local resultfile=$path"/resultfile"
	local statusfile=$path"/statusfile"
	local okfile=$path"/okfile"
	local tempcmdfile=$path"/tempcmd.txt"
	local sw=$1
	local command=$2
	local hashsize=$3
	local returncode="1"
	local data

	if [[ -f $resultfile ]] ; then 
		rm $resultfile
	fi
	if [[ -f $statusfile ]] ; then 
		rm $statusfile
	fi
	if [[ -f $okfile ]] ; then 
		rm $okfile
	fi

	if [[ "$sw" == "1" ]] ; then
		echo "cmd" > $tempcmdfile
	elif [[ "$sw" == "2" ]] ; then
		echo "down" > $tempcmdfile
	elif [[ "$sw" == "3" ]] ; then
		echo "up" > $tempcmdfile
	elif [[ "$sw" == "4" ]] ; then
		echo "hash" > $tempcmdfile
	elif [[ "$sw" == "5" ]] ; then
		echo "localhash" > $tempcmdfile
		echo $hashsize >> $tempcmdfile	
	fi
	
	echo "$command" >> $tempcmdfile		
		
	mv $tempcmdfile $cmdfile	

	while true; do
		if [[ ! -f $cmdfile ]] ; then
			
			if [[ -f $statusfile ]] ; then
				returncode=$(cat $statusfile)
			fi
			
			if [[ -f $resultfile ]] ; then
				data=$(<$resultfile)
				echo "$data"
			fi

			break
			
		fi
	done

	return "$returncode"
}

#------------------------------------- CHECK NETWORK -----------------------------------------

run_and_check_client(){
	local path="$glb_memtemp_local"
	local statusfile=$path"/statusfile"
	local nonreadyfile=$path"/nonreadyfile"
	local curdir=$(pwd)
	local clientisrunning
	local endwhile=0
	local count=1
	local start_time
	local elapsed
	
	rm $statusfile
	touch $statusfile

	if [[ $glb_endclienttox_pid != 0 ]] ; then
		clientisrunning=$(ps $glb_endclienttox_pid | grep client_by_tox_protocol)	
	fi

	if [[ ! "$clientisrunning" ]] ; then	
		echo "client is not running, need run client"
	
		cd "$glb_clienttox_local"

		rm "$nonreadyfile"

		./client_by_tox_protocol & glb_endclienttox_pid=$!

		# "id:""$glb_endclienttox_pid"

		start_time=$SECONDS

		# $curdir
		
		cd $curdir

	
		while true; do
		
			elapsed=$(( SECONDS - start_time ))
			
			if [[ $elapsed > 30 ]] && [[ ! -f  $nonreadyfile ]] ; then
				endwhile=1
			fi

			if [[ $endwhile -eq 1 ]] ; then 
				break 
			else 
				# "sleep2s"
				sleep 2
				count=$(( $count + 1 ))
				#echo "count:""$count"
				if [[ $count -eq 900 ]] ; then
					return 1
				fi
			fi
				
		done
	fi


	#echo "client is ready"

	return 0
}

#------------------------------------- MERGE PHASE -----------------------------------------

merge_from_server(){
	local dir1="$1"
	local dir2="$2"
	local interpath="$3"
	local pathname=$(echo "$dir2""$interpath" | tr -d '\n' | xxd -pu -c 1000000)
	local rs code
	local outputfile="outputlistdirfile.txt"
	local temp_name
	local temp_type
	local temp_headhash
	local temp_mtime
	local temp_size
	
	local s k t

	declare -a name
	declare -a type
	declare -a headhash
	declare -a mtime
	declare -a size

	mech "beginwith""$dir1""----""$dir2""----""$interpath"
	rs=$(run_command_in_remote "1" "bash ${glb_memtemp_remote}/${glb_getlistdirfiles_remote} ${pathname} ${glb_memtemp_remote}/${outputfile}")
	code=$?
	mech "generate outputfile:""${code}"

	if [[ $code -ne 0 ]] ; then
		return 1
	fi
	
	rm "$glb_memtemp_local""/""$outputfile"
	rm "$glb_memtemp_local""/tempfile"

	rs=$(run_command_in_remote "2" //.///${outputfile})
	code=$?
	mech "down outputfile:""${code}"

	if [[ $code -ne 0 ]] ; then
		return 1
	else
		if [[ -f $glb_memtemp_local/tempfile ]] ; then
			mv $glb_memtemp_local/tempfile $glb_memtemp_local/${outputfile}
		else
			#zero file
			return 0
		fi
	fi

	count=0
	while IFS=/ read beforeslash afterslash_1 afterslash_2 afterslash_3 afterslash_4 afterslash_5
	do
		name[$count]="$afterslash_1"
		type[$count]="$afterslash_2"
		mtime[$count]="$afterslash_3"
		size[$count]="$afterslash_4"
		headhash[$count]="$afterslash_5"
		count=$(($count + 1))
	done < "$glb_memtemp_local"/"$outputfile"
	
	#for i in "${!name[@]}" ; do
	#	mech "name:""${name[$i]}"
	#	mech "type:""${type[$i]}"
	#	mech "mtime:""${mtime[$i]}"
	#	mech "size:""${size[$i]}"
	#	mech "headhash:""${headhash[$i]}"
	#	mech "--------------------------"
	#done	
	

	for i in "${!name[@]}" ; do
		if [[ "${type[$i]}" != "d" ]] ; then
			#mech "check filename:""${name[$i]}"
			mech "check filename:""$dir1""$interpath""/""${name[$i]}"

			if [[ ! -f "$dir1""$interpath""/""${name[$i]}" ]] ; then
				rm "$glb_memtemp_local"/tempfile
				rs=$(run_command_in_remote "2" //x//"$interpath""/""${name[$i]}")
				code=$?

				if [[ $code -ne 0 ]] ; then
					return 1
				fi

				mech "down outputfile"

				if [[ "$code" == "0" ]] ; then
					mv $glb_memtemp_local/tempfile "$dir1""$interpath""/""${name[$i]}"
					s=$(stat -c %y "$dir1""$interpath""/""${name[$i]}")
					k="$dir2""$interpath""/""${name[$i]}"
					t="touch -d \""${s}"\" \""${k}"\""
					mech "t:""${t}"

					rs=$(run_command_in_remote "1" "${t}")
					code=$?

					if [[ $code -ne 0 ]] ; then
						return 1
					fi
				fi
			fi
		fi
	done
	

	for i in "${!name[@]}" ; do
		if [[ "${type[$i]}" == "d" ]] ; then
			if [[ ! -d "$dir1""$interpath""/""${name[$i]}" ]] ; then
				if [[ -d "$dir1""$interpath" ]] ; then
					mkdir "$dir1""$interpath""/""${name[$i]}"
				fi
			fi

			merge_from_server "$dir1" "$dir2" "$interpath""/""${name[$i]}"
			code=$?
			
			if [[ $code -ne 0 ]] ; then
				return 1
			fi
		fi
	done

	return 0
}

#------------------------------ FIND SAME FILE --------------------------------

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

	rs=$(run_command_in_remote "3" "//,//$glb_memtemp_local"/"$listfiles")
	code=$?

	if [[ "$code" != "0" ]] ; then
		return 1
	fi	
	

	pathname=$(echo "$dir2" | tr -d '\n' | xxd -pu -c 1000000)

	rs=$(run_command_in_remote "1" "bash ${glb_memtemp_remote}/${glb_compare_listfile_inremote} ${listfiles} ${pathname} ${outputfile_inremote}")
	code=$?
	mech "generate new outputfile ""$code"

	if [[ "$code" != "0" ]] ; then
		return 1
	fi

	rm $glb_memtemp_local/tempfile
	rm $glb_memtemp_local/${outputfile_inremote}

	rs=$(run_command_in_remote "2" "//.///""$outputfile_inremote")
	code=$?
	mech "getback outputfile ""$code"
	
	if [[ "$code" == "0" ]] ; then
		if [[ -f $glb_memtemp_local/tempfile ]] ; then
			mv $glb_memtemp_local/tempfile $glb_memtemp_local/${outputfile_inremote}
			return 0
		else
			return 255
		fi
	else 
		return 1
	fi
}

#------------------------------ FIND SAME DIR --------------------------------

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

	rs=$(run_command_in_remote "3" "//,//$glb_memtemp_local"/"$listfiles")
	code=$?

	if [[ "$code" != "0" ]] ; then
		return 1
	fi

	pathname=$(echo "$dir2" | tr -d '\n' | xxd -pu -c 1000000)

	rs=$(run_command_in_remote "1" "bash ${glb_memtemp_remote}/${glb_compare_listdir_inremote} ${listfiles} ${pathname} ${outputdir_inremote}")
	code=$?
	mech "generate new outputdir ""$code"

	if [[ "$code" != "0" ]] ; then
		return 1
	fi

	rm $glb_memtemp_local/tempfile
	rm $glb_memtemp_local/${outputdir_inremote}

	rs=$(run_command_in_remote "2" "//.///""$outputdir_inremote")
	code=$?
	mech "getback outputdir ""$code"
	
	if [[ "$code" == "0" ]] ; then
		if [[ -f $glb_memtemp_local/tempfile ]] ; then
			mv $glb_memtemp_local/tempfile $glb_memtemp_local/${outputdir_inremote}
			return 0
		else 
			return 255
		fi
	else 
		return 1
	fi
}

#------------------------------ APPEND FILE --------------------------------

append_file_with_hash_checking(){
	local dir1="$1"
	local dir2="$2"
	local interpath="$3"
	local filename="$4"
	local hashremotefile
	local hashlocalfile
	local filesize
	local mtime
	local tg

	local temphashfilename="tempfile.totalmd5sum.being"

	rm "$glb_memtemp_local"/"$temphashfilename"	

	result=$(run_command_in_remote "4" "//x//${interpath}/${filename}")
	
	code=$?

	hashremotefile="$result"

	tg="wc -c ""$glb_mainmem_remote""${interpath}""/""${filename}"" | awk ""'{print "'$1'"}'"

	result=$(run_command_in_remote "1" "${tg}")
	
	code=$?

	filesize="$result"

	# "$hashremotefile"

	# "filesize:""$filesize"

	if [[ -f "$dir1""$interpath"/"$filename" ]] ; then		
		result=$(run_command_in_remote "5" "$glb_mainmem_local""${interpath}""/""${filename}" "$filesize")
	
		code=$?

		hashlocalfile="$result"
		# "hashlocal:""$hashlocalfile"
		
		if [[ "$hashlocalfile" == "$hashremotefile" ]] ; then
			mech 'has same md5hash after truncate-->continue append'
			mtime=$(stat "${glb_mainmem_local}${interpath}/${filename}" -c %Y)
			if [[ -z "$mtime" ]] ; then
				mech 'file not found'
				return 252				
			else
				append_native_file "$interpath" "$filename" 0 "$mtime"
				code="$?"				
				return "$code"
			fi
		else
			mech 'different md5hash after truncate-->copy total file'
			# "$interpath"
			# "$filename"
			copy_file_to_remote "$interpath" "$filename"
		fi
	fi

}

append_native_file(){
	local pathtofile=$1
	local filename=$2
	local wantcopy=$3
	local mtimebeforeup=$4
	local rs code
	local s k t
	local mtimeafterup

	if [[ "$wantcopy" == "1" ]] ; then
		rs=$(run_command_in_remote "1" "test -f $glb_memtemp_remote/tempfile")
		code=$?

		if [[ "$code" == "0" ]] ; then			
			rs=$(run_command_in_remote "1" "rm $glb_memtemp_remote/tempfile")
			code=$?

			if [[ "$code" != "0" ]] ; then
				return 255
			fi
		#code = 1 nghia la tempfile ko ton tai, != 1 la loi khac
		elif [[ "$code" != "1" ]] ; then
			return 255
		fi
	fi

	rs=$(run_command_in_remote "3" "//x//${pathtofile}/${filename}")
	code=$?

	if [[ "$code" != "0" ]] ; then
		return 1;
	fi

	mtimeafterup=$(stat "${glb_mainmem_local}${pathtofile}/${filename}" -c %Y)

	if [[ "$mtimebeforeup" == "$mtimeafterup" ]] ; then

		s=$(stat -c %y "${glb_mainmem_local}${pathtofile}/${filename}")
		k="${glb_mainmem_remote}${pathtofile}/${filename}"
		
		#if [[ "$pathtofile" == *\'* ]] || [[ "$filename" == *\'* ]]; then
		t="touch -d \""${s}"\" \""${k}"\""
		#else
		#  t="touch -d '"${s}"' '"${k}"'"
		#fi

		#t="touch -d '"${s}"' '"${k}"'"
		mech "t:""${t}"

		rs=$(run_command_in_remote "1" "${t}")
		code=$?	

		if [[ "$code" != "0" ]] ; then
			return 1
		else
			return 0
		fi
	else
		return 254
	fi
}

#----------------------------------------COPY---------------------------------------------

copy_file_to_remote() {
	local pathtofile=$1
	local filename=$2
	local regardlesstempfile=$3
	local mtime
	local code
	
	mtime=$(stat "${glb_mainmem_local}${pathtofile}/${filename}" -c %Y)
	
	if [[ -z "$mtime" ]] ; then
		mech 'file not found --> mtime not found'
		return 255		
	else
		append_native_file "$pathtofile" "$filename" $regardlesstempfile "$mtime"
		code="$?"
		return "$code"
	fi
}

#-------------------------------------SYNC-----------------------------------------

sync_dir(){
	local param1="$glb_mainmem_local"
	local param2="$glb_mainmem_remote"
	local rs code
	local findresult
	local count
	local total
	local beforeslash
	local outputfile_inremote="outputfile_inremote.txt"
	local outputdir_inremote="outputdir_inremote.txt"
	local relativepath="$1"
	local element=$(basename "$param1")
	local afterslash_1
	local afterslash_2
	local afterslash_3
	local afterslash_4
	local afterslash_5
	local afterslash_6
	local afterslash_7
	
	# declare array
	declare -a dirname
	
	# declare array
	declare -a name
	declare -a size
	declare -a md5hash
	declare -a mtime
	declare -a mtime_local
	declare -a apporcop
	
	# declare array
	local countother
	declare -a nameother
	declare -a statusother
	
	mech "relativepath:""$relativepath"";compare ""$param1""$relativepath"" ""$param2""$relativepath"

	find_same_files_in_list "$param1""$relativepath" "$param2""$relativepath"
	code=$?

	if [[ "$code" == "1" ]] ; then
		return 1
	fi

	#cat "$glb_memtemp_local""/""$outputfile_inremote"

	if [[ -f "$glb_memtemp_local""/""$outputfile_inremote" ]] ; then
		count=0
		countother=0
		total=0
		while IFS=/ read -r beforeslash afterslash_1 afterslash_2 afterslash_3 afterslash_4 afterslash_5 afterslash_6 afterslash_7
		do
			if [[ "$afterslash_1" != "" ]] ; then
				if [[ "$afterslash_2" == "0" ]] ; then
					name[$count]="$afterslash_1"
					size[$count]="$afterslash_4"
					md5hash[$count]="$afterslash_5"
					mtime[$count]="$afterslash_6"
					mtime_local[$count]="$afterslash_7"
					mech "needappend:""${name[$count]}""-----""${size[$count]}""-----""${md5hash[$count]}""-----""${mtime[$count]}"
					apporcop[$count]=1
					count=$(($count + 1))
				elif [[ "$afterslash_2" == "4" ]] || [[ "$afterslash_2" == "5" ]] ; then
					name[$count]="$afterslash_1"
					size[$count]="$afterslash_4"
					md5hash[$count]="$afterslash_5"
					mtime[$count]="$afterslash_6"
					mtime_local[$count]="$afterslash_7"
					mech "needcopy:""${name[$count]}""-----""${size[$count]}""-----""${md5hash[$count]}""-----""${mtime[$count]}"
					apporcop[$count]=45
					count=$(($count + 1))
				else
					nameother[$countother]="$afterslash_1"
					statusother[$countother]="$afterslash_2"
					countother=$(($countother + 1))
				fi
				
				if [[ "$afterslash_2" != "3" ]] ; then
					total=$(($total + 1))
				fi
			else
				mech "--------------------""$total"" files received valid---------------------"
			fi
		done < "$glb_memtemp_local""/""$outputfile_inremote"

		count=0
		for i in "${!nameother[@]}"
		do
			#printf '%s status: %s\n' "${nameother[$i]}" "${statusother[$i]}" 
			count=$(($count + 1))
		done
		mech 'file ko duoc tinh------------'"$count"

		count=0
		for i in "${!name[@]}"
		do
			#echo "${name[$i]}"

			#findresult=$(find "$param1""$relativepath" -maxdepth 1 -type f -name "${name[$i]}")
			
			#code=$?
			if [[ -f "$param1""$relativepath""/""${name[$i]}" ]] ; then
				code=0
			else
				code=1				
			fi
			
			#neu tim thay
			#if [[ "$code" == "0" ]] && [[ "$findresult" ]] ; then
			if [[ "$code" == "0" ]] ; then
				# "nhung file giong ten nhung khac attribute:""$findresult"
				if [[ "${apporcop[$i]}" == "1" ]] ; then
					#file local da bi modify (ko ro vi tri bi modify) ---> append with hash
					mech "->append:""mtimelc:""${mtime_local[$i]}"" mtime:""${mtime[$i]}""-""$param1"" ""$param2"" ""${name[$i]}"" ""${size[$i]}"					
				else
					mech "->copy:""$param1"" ""$param2"" ""${name[$i]}"
				fi
				
				while true; do
					rs=$(copy_file_to_remote "$relativepath" "${name[$i]}" 0)
					code="$?"

					if [[ "$code" == "1" ]] ; then
						#try to stop sync
						echo "cannotcp   ""$relativepath""/""${name[$i]}" >> "$errorlogfile"
						mech "cannotcp   ""$relativepath""/""${name[$i]}"" view errorlog for detail"
						break
					elif [[ "$code" == "254" ]] ; then
						glb_befDirHash="none"
						break
					else
						break
					fi
				done
			#neu ko tim thay
			else
				mech '**********************************file not found'
			fi
			count=$(($count + 1))
		done
		
		mech "--------------------""$count"" files can append hoac copy ---------------------"
	fi

	#dong bo thu muc 
	find_same_dirs_in_list "$param1""$relativepath" "$param2""$relativepath"
	code=$?

	if [[ "$code" == "1" ]] ; then
		return 1
	fi

	if [[ -f "$glb_memtemp_local""/""$outputdir_inremote" ]] ; then
		unset beforeslash
		unset afterslash_1
		unset afterslash_2
		unset afterslash_3
		count=0
		while IFS=/ read beforeslash afterslash_1 afterslash_2 afterslash_3
		do
			# "$afterslash_1"";""$afterslash_2"
			if [[ "$afterslash_1" != "" ]] ; then
				if [[ "$afterslash_2" != "5" ]] ; then
					dirname[$count]="$afterslash_1"
					count=$(($count + 1))				
				fi
			fi
		done < "$glb_memtemp_local""/""$outputdir_inremote"

		for i in "${!dirname[@]}"
		do
			# "path:""$param1""$relativepath"/"${dirname[$i]}"
			# "$relativepath"/"${dirname[$i]}"

			glb_befDirHash=$(stat "$param1""$relativepath""/""${dirname[$i]}" -c '%Y')
			echo "-----------------------syncdir:   ""$param1""$relativepath""/""${dirname[$i]}"
			echo "$relativepath"/"${dirname[$i]}" >> "$hashlogfile"
			echo "$glb_befDirHash" >> "$hashlogfile"
			
			sync_dir "$relativepath"/"${dirname[$i]}"
			code="$?"
			
			if [[ "$code" == "1" ]] ; then
				return 1
			fi
		done
	fi

	return 0
}

#-------------------------------------MAIN-----------------------------------------
: '
test_recursive(){
	local path=$1
	local element=$(basename "$path")
	local relativepath
	local level=$3	

	if [ $level -eq 0 ] ; then
		relativepath=""
	else
		relativepath="$2""/""$element"
	fi

	level=$(($level + 1))
	# hien thi $relativepath

	for d in $path/* ; do
		if [ -d "$d" ] ; then
			test_recursive "$d" "$relativepath" $level
		fi
	done
}
'

get_dir_hash(){
	local dir_ori="$1"
	local relative_path="$2"
	local pathname
	local dname
			
	for pathname in "$dir_ori"/* ; do
		if [[ -d "$pathname" ]] ; then 
			dname=$(basename "$pathname")
			echo "$relative_path""/""$dname" >> "$testhashlogfile"
			glb_afDirHash=$(stat "$pathname" -c '%Y')
			echo "$glb_afDirHash" >> "$testhashlogfile"			
			get_dir_hash "$pathname" "$relative_path""/""$dname"
		fi
	done
}

main(){
	local rs
	local code
	local flag	
	local now
	
	if [[ ! -f "$mainlogfile" ]] ; then
		touch "$mainlogfile"
	else
		truncate --size=0 "$mainlogfile"
	fi
	
	if [[ ! -f "$errorlogfile" ]] ; then
		touch "$errorlogfile"
	else
		truncate --size=0 "$errorlogfile"
	fi
	
	now=$(date)
	echo "start syncing...""$now" >> "$mainlogfile"
	
	while true; do

		flag=0

		run_and_check_client	

		code=$?

		if [[ "$code" == "1" ]] ; then
			echo '###error###' >> "$mainlogfile"
			echo 'sleep 3 phut'
			sleep 180
			continue
		fi

		echo "---------------------------------------merge phase------------------------------------------"
		
		while true; do
			if [[ ! -f "$glb_stopmerge" ]] ; then
				merge_from_server "$glb_mainmem_local" "$glb_mainmem_remote" ""
				code=$?
				
				rs=$(ps -p $glb_endclienttox_pid | sed -n 2p)
				
				if [[ -z "$rs" ]] ; then	
					# "$glb_endclienttox_pid is not running"
					flag=1
					break								
				else
					# "$glb_endclienttox_pid is running"
					if [[ $code -eq 0 ]] ; then
						touch "$glb_stopmerge"
						break
					else 
						#nghi 5 phut
						echo 'sleep 5m in merge phase' 
						sleep 300
						continue
					fi
				fi
			else			
				break
			fi
		done

		if [[ $flag -eq 1 ]] ; then
			kill $glb_endclienttox_pid;
			#nghi 1 phut 
			sleep 60
			continue
		fi
		
		#append_native_file "/hello" "tôi o3.mp4" 0 ""

		#find_same_files_in_list /..../MainDir/dir1/d1indir1 /var/res/share/syncdir/dir1/d1indir1				

		#find_same_dirs_in_list "/..../TestSyncDir/MainDir" "/var/res/share/syncdir"					

		#copy_file_to_remote "/hello" "tôi o3.mp4" 0

		#append_file_with_hash_checking "/.../TestSyncDir/MainDir" "/var/res/share/syncdir" "/hello" "tôi o3.mp4"

		echo "---------------------------------------sync phase------------------------------------------"
		
		if [[ -f "$hashlogfile" ]] ; then
			while true; do
				if [[ ! -f "$testhashlogfile" ]] ; then
					touch "$testhashlogfile"
				else
					truncate --size=0 "$testhashlogfile"
				fi
				
				glb_afDirHash=$(stat "$glb_mainmem_local" -c '%Y')
				echo "/" >> "$testhashlogfile"
				echo "$glb_afDirHash" >> "$testhashlogfile"
				get_dir_hash "$glb_mainmem_local" ""
				rs=$(diff "$hashlogfile" "$testhashlogfile")
				
				if [[ -z "$rs" ]] ; then
					echo 'sleep 2 phut'"----justbeforehash:""$glb_afDirHash"
					echo '###ok###' >> "$mainlogfile"
					sleep 120					
				else							
					break
				fi
			done
		fi
	
		while true; do
			if [[ ! -f "$hashlogfile" ]] ; then
				touch "$hashlogfile"
			else
				truncate --size=0 "$hashlogfile"
			fi
			glb_befDirHash=$(stat "$glb_mainmem_local" -c '%Y')
			echo "/" >> "$hashlogfile"
			echo "$glb_befDirHash" >> "$hashlogfile"
			sync_dir ""
			code=$?		
			# "code after sync_dir:""$code"
			rs=$(ps -p $glb_endclienttox_pid | sed -n 2p)
			#glb_befDirHash=$(echo "$glb_befDirHash" | md5sum )
			# "rs:""$rs""----beforehash:""$glb_befDirHash"

			if [[ -z "$rs" ]] ; then	
				echo '###error2###' >> "$mainlogfile"
				echo "$glb_endclienttox_pid is not running, sleep 2 phut"
				#nghi 2 phut 
				sleep 120
				break				
			else
				# "$glb_endclienttox_pid is running"
				if [[ $code -eq 0 ]] ; then
					while true; do
						if [[ ! -f "$testhashlogfile" ]] ; then
							touch "$testhashlogfile"
						else
							truncate --size=0 "$testhashlogfile"
						fi
						
						glb_afDirHash=$(stat "$glb_mainmem_local" -c '%Y')
						echo "/" >> "$testhashlogfile"
						echo "$glb_afDirHash" >> "$testhashlogfile"
						get_dir_hash "$glb_mainmem_local" ""
						rs=$(diff "$hashlogfile" "$testhashlogfile")
						if [[ -z "$rs" ]] ; then
							echo 'sleep 2 phut'"----afterhash:""$glb_afDirHash"
							echo '###ok###' >> "$mainlogfile"
							sleep 120							
						else			
							#exit
							break
						fi
					done
				else 
					#nghi 1 phut 
					echo '###error1###' >> "$mainlogfile"
					echo 'sleep 2 phut'
					sleep 120
				fi

				continue
			fi
		done

		kill $glb_endclienttox_pid;					

	done

	#trap "echo 'trap stop signal'; kill $glb_endclienttox_pid; exit" SIGHUP SIGINT SIGTERM
	kill $glb_endclienttox_pid;
}


main
