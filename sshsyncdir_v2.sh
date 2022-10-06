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

glb_file_track_dir="$glb_memtemp_local"/trackdirlog.txt
glb_file_track_file="$glb_memtemp_local"/trackfilelog.txt
	
glb_newpath_for_rerun=""
glb_rerun_with_newpath=0
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

contain_special_character_Win_forbidden(){
	local prm=$1

	if [[ "$prm" == *\** ]] || [[ "$prm" == *\"* ]] || [[ "$prm" == *\?* ]] || [[ "$prm" == *\<* ]] || [[ "$prm" == *\>* ]] || [[ "$prm" == *\:* ]] || [[ "$prm" == *\\* ]] || [[ "$prm" == *\|* ]] ; then
			#co chua
			return 0	
	else
			#ko chua
			return 1
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
	local rs
	
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
				
				if [[ -f $resultfile ]] ; then
					data=$(<$resultfile)
					echo "$data"
				fi	
			else
				returncode=1			
			fi					

			break
		else
			rs=$(ps -p $glb_endclienttox_pid | sed -n 2p)
				
			if [[ -z "$rs" ]] ; then	
				return 1								
			fi
			
			sleep 1
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
			
			if [[ $elapsed > 10 ]] && [[ ! -f  $nonreadyfile ]] ; then
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
			md5hash=$(head -c 8192 "$dir1"/"$pathname" | md5sum | awk '{ print $1 }')
			#md5tailhash=$(get_src_content_file_md5sum "$pathname")
			#mtime_temp=$(stat "$dir1"/"$pathname" --printf='%y\n')
			#mtime=$(date +'%s' -d "$mtime_temp")
			mtime=$(stat "$dir1"/"$pathname" -c %Y)
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
	#echo "pathname_encoded:""$pathname"
	
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
			
			contain_special_character_Win_forbidden "$pathname"			
			code=$?
			#neu chua ki tu cam
			if [[ $code -eq 0 ]] ; then
				echo "cannot sync dir with forbidden character:""$dir1""/""$pathname" >> "$errorlogfile"
				continue
			fi		
			
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
		echo "?/?" >> "$glb_memtemp_local"/"$listfiles"	
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

append_native_file(){
	local pathtofile=$1
	local filename=$2
	local wantcopy=$3
	local mtimebeforeup=$4
	local rs code
	local s k t filesize hashoflocalfile
	local mtimeafterup	
	local truncatesize=1000000

	#get filesize	
	filesize=$(run_command_in_remote "1" "if [ -f ${glb_memtemp_remote}/tempfile ] ; then fs=\$(stat -c %s ${glb_memtemp_remote}/tempfile); echo \$fs; else echo 0; fi")
	code=$?	
	if [[ "$code" != "0" ]] ; then		
		return 1			
	fi
	
	if [[ -z "$filesize" ]] ; then
		return 1
	fi
	
	mech "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^filesize of tempfile:""$filesize"
	k=0
	s=$(( filesize/truncatesize ))
	if [[ $s -gt 1 ]] ; then
		s=$(( $s - 1 ))
		hashoflocalfile=$(dd if="${glb_mainmem_local}${pathtofile}/${filename}" bs=1MB count=1 skip=${s} | md5sum)	
		rs=$(run_command_in_remote "1" "if [ -f ${glb_memtemp_remote}/tempfile ] ; then rs=\$(dd if=${glb_memtemp_remote}/tempfile bs=1MB count=1 skip=${s} | md5sum); if [ \"\$rs\" = \"${hashoflocalfile}\" ] ; then echo bang; else echo khongbang; fi; else echo notfound; fi")
		code=$?
		if [[ "$code" != "0" ]] ; then		
			return 1			
		fi
		t=$(echo "$rs" | tail -n 1)
		if [[ "$t" == "bang"  ]] ; then
			k=1
			mech "^^^^^^^^^^bang"
		fi		
	fi
	
	
	#xoa tempfile
	if [[ $k -eq 0 ]] ; then
		rs=$(run_command_in_remote "1" "if [ -f ${glb_memtemp_remote}/tempfile ] ; then rm ${glb_memtemp_remote}/tempfile; fi")
		code=$?	
		if [[ "$code" != "0" ]] ; then		
			return 1			
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

append_file_to_remote() {
	local pathtofile=$1
	local filename=$2
	local regardlesstempfile=$3
	local mtime
	local code rs
	
	mtime=$(stat "${glb_mainmem_local}${pathtofile}/${filename}" -c %Y)
	code=$?
	if [[ $code -ne 0 ]] ; then
		mech "cannotappend   ""file ""${glb_mainmem_local}${pathtofile}/${filename}""not found --> mtime not found"
		return 255		
	else
		rs=$(run_command_in_remote "1" "mv \"${glb_mainmem_remote}${pathtofile}/${filename}\" ${glb_memtemp_remote}/tempfile")
		code=$?
		if [[ "$code" != "0" ]] ; then		
			return 1			
		fi
		rs=$(run_command_in_remote "1" "touch \"${glb_mainmem_remote}${pathtofile}/${filename}.tMpZ.in.being.appended\"")
		code=$?
		if [[ "$code" != "0" ]] ; then		
			return 1			
		fi
		append_native_file "$pathtofile" "$filename" $regardlesstempfile "$mtime"
		code="$?"
		if [[ "$code" == "0" ]] ; then		
				rs=$(run_command_in_remote "1" "rm \"${glb_mainmem_remote}${pathtofile}/${filename}.tMpZ.in.being.appended\"")
		fi
		return "$code"
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
	code=$?
	if [[ $code -ne 0 ]] ; then
		mech "cannotcp   ""file ""${glb_mainmem_local}${pathtofile}/${filename}""not found --> mtime not found"
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
					while true; do
						rs=$(append_file_to_remote "$relativepath" "${name[$i]}" 0)
						code="$?"
						
						if [[ "$code" == "1" ]] ; then
							#try to stop sync
							echo "cannotappend   ""$relativepath""/""${name[$i]}" >> "$errorlogfile"
							mech "cannotappend   ""$relativepath""/""${name[$i]}"" view errorlog for detail"
							break
						elif [[ "$code" == "254" ]] ; then
							mech "cannotappend   ""filecp vua bi thay doi   ""$relativepath""/""${name[$i]}"
							break
						else
							break
						fi
					done
				else
					mech "->copy:""$param1"" ""$param2"" ""${name[$i]}"
					while true; do
						rs=$(copy_file_to_remote "$relativepath" "${name[$i]}" 0)
						code="$?"
						
						if [[ "$code" == "1" ]] ; then
							#try to stop sync
							echo "cannotcp   ""$relativepath""/""${name[$i]}" >> "$errorlogfile"
							mech "cannotcp   ""$relativepath""/""${name[$i]}"" view errorlog for detail"
							break
						elif [[ "$code" == "254" ]] ; then
							mech "cannotcp   ""filecp vua bi thay doi   ""$relativepath""/""${name[$i]}"
							break
						else
							break
						fi
					done
				fi
				
				
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

			#glb_befDirHash=$(stat "$param1""$relativepath""/""${dirname[$i]}" -c '%Y')
			#echo "-----------------------syncdir:   ""$param1""$relativepath""/""${dirname[$i]}"
			#echo "$relativepath"/"${dirname[$i]}" >> "$hashlogfile"
			#echo "$glb_befDirHash" >> "$hashlogfile"
			
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
checkmodifydir(){
	# declare array
	declare -a strarr_main
	declare -a strarr_cur
	
	# set comma as delimiter
	local IFS='/'
	# local param
	local len_main line fs1 fs2 i j passdir passfile
	local folder=$1
	local curdir=$(pwd)
	local time_lastrun=$2
	
	cd "$folder"
	mech "time_lastrun:""$time_lastrun"
	find . -type d -mmin -$time_lastrun > "$glb_file_track_dir"
	find . -type f -mmin -$time_lastrun > "$glb_file_track_file"
	cd "$curdir"
	
	#for dirs
	fs1=$(stat -c %s "$glb_file_track_dir")
	
	#init
	read -a strarr_main <<< ""
	len_main=0
	passdir=0
	
	if [[ ! -f  "$glb_file_track_dir" ]] ; then
		exit 2
	fi
	
	if [[ $fs1 -eq 0 ]] ; then
		passdir=1
	fi
	
	if [[ $passdir -eq 0 ]] ; then
		while read -r line; 
		do 
			read -a strarr_cur <<< "$line"
			
			if [[ $len_main -gt 0 ]] ; then
				j=0
				for i in "${!strarr_cur[@]}" ; do	
					if [[ $j -lt $len_main ]] && [[ "${strarr_cur[$i]}" == "${strarr_main[$j]}" ]] ; then
						j=$(( $j + 1 ))
					else
						break;
					fi
				done
				len_main=$j
			else
				unset strarr_main
				read -a strarr_main <<< "$line"
				len_main=${#strarr_main[@]}
			fi
			
			unset strarr_cur		
		done < "$glb_file_track_dir"
	fi
	
	#for files
	fs2=$(stat -c %s "$glb_file_track_file")

	if [[ ! -f  "$glb_file_track_file" ]] ; then
		exit 2
	fi
	
	if [[ $fs2 -eq 0 ]] ; then
		passfile=1
	fi

	if [[ $passfile -eq 0 ]] ; then
		while read -r line; 
		do 
			read -a strarr_cur <<< "$line"
			
			if [[ $len_main -gt 0 ]] ; then
				j=0
				for i in "${!strarr_cur[@]}" ; do	
					if [[ $j -lt $len_main ]] && [[ "${strarr_cur[$i]}" == "${strarr_main[$j]}" ]] ; then
						j=$(( $j + 1 ))
					else
						break;
					fi
				done
				len_main=$j
			else
				unset strarr_main
				read -a strarr_main <<< "$line"
				len_main=${#strarr_main[@]}
			fi
			
			unset strarr_cur		
		done < "$glb_file_track_file"
	fi
	
	if [[ $passdir -eq 1 ]] && [[ $passfile -eq 1 ]] ; then
		exit 1
	fi
	
	outstr=""
	for i in "${!strarr_main[@]}" ; do			
		if [[ $i -lt $len_main ]]; then
			if [[ $i -eq 0 ]] ; then
				outstr=""
			else
				outstr=$(echo "$outstr""/""${strarr_main[$i]}")
			fi
		fi
	done

	echo "$outstr"
	
	exit 0
}

get_dir_hash(){
	local dir_ori="$1"
	local relative_path="$2"
	local pathname
	local dname
			
	for pathname in "$dir_ori"/* ; do
		if [[ -d "$pathname" ]] ; then 
			dname=$(basename "$pathname")
			
			contain_special_character_Win_forbidden "$dname"			
			code=$?
			#neu chua ki tu cam
			if [[ $code -eq 0 ]] ; then				
				continue
			fi
				
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
	local elapsed
	local start_time=$SECONDS
	local i j k
	
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
				code=$?
				if [[ $code -ne 0 ]] ; then
					#file not found
					echo "firstime: hashfilelog or testhashfilelog not found, rerun sync"
					break
				fi
				
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

			if [[ $glb_rerun_with_newpath -eq 1 ]] ; then
				rs=$(run_command_in_remote "1" "if [ -d \"${glb_mainmem_remote}${glb_newpath_for_rerun}\" ] ; then echo co; else echo khongco; fi")
				code=$?
				#echo "ket qua tra ve cua cmd truoc khi exit:""$rs"
				#exit 0
				if [[ "$code" != "0" ]] ; then	
					mech "rerun with newpath: ko ton tai duong dan phia remote"
					sync_dir ""
					code=$?
				else
					mech "rerun with newpath: ok"
					sync_dir "$glb_newpath_for_rerun"
					code=$?
				fi
				glb_rerun_with_newpath=0
			else
				sync_dir ""
				code=$?
			fi			
					
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
						
						elapsed=$(( $SECONDS - $start_time ))
						
						if [[ $elapsed -lt 120 ]] ; then
							#hai file hashfilelog va testhashfilelog giong nhau
							echo 'sleep 2 phut'"----aftersync"
							echo '###ok###' >> "$mainlogfile"
							sleep 120
						fi
						
						elapsed=$(( $SECONDS - $start_time ))
						i=$(( $elapsed/60 + 1 ))
						start_time=$SECONDS							
						rs=$(checkmodifydir "$glb_mainmem_local" "$i")
						code=$?

						if [[ "$code" == "1" ]] ; then						
							continue
						elif [[ "$code" == "2" ]] ; then
							echo "rerun with fulldir "
							mech "rerun with fulldir "
							break
						elif [[ "$code" == "0" ]] ; then										
							glb_newpath_for_rerun="$rs"
							glb_rerun_with_newpath=1
							echo "rerun with newpath ""$glb_newpath_for_rerun"
							mech "rerun with newpath ""$glb_newpath_for_rerun"
							break
						fi
													
					done
				else 
					#nghi 1 phut 
					echo '###error1###' >> "$mainlogfile"
					echo 'sleep 2 phut do sync fail'
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
