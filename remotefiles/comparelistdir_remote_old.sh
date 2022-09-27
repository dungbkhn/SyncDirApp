#!/bin/bash
#./test3.sh listdirsforcmp.txt "/home/dungnt/ShellScript/thư mục so sánh" outputdir_inremote.txt
shopt -s dotglob
shopt -s nullglob

memtemp=/var/res/backup/.Temp
#memtemp=/home/dungnt/ShellScript/sshsyncapp/.temp
workingdir=$(pwd)

#ten file chua ds file tu phia local
param1=$1
#thu muc dang sync phia remote
param2=$(echo "$2" | tr -d '\n' | xxd -r -p)
#param2=$2
#outputfilenamefordir 
param3=$3

# declare array
declare -a name
declare -a subname
declare -a subnameisfile
declare -a level
declare -a hassamedir

declare -a name_remote
declare -a hassamedir_remote
declare -a maxsavelinkedname_remote
declare -a maxsavelinkedid_remote
declare -a maxsubcount_remote

declare -a tempsubname
declare -a tempsubnameisfile

declare -a tempsubname_remote
declare -a tempsubnameisfile_remote

declare -a rs

echo "$param2"
echo "$memtemp""/""$param1"

if [[ ! -f "$memtemp""/""$param1" ]] ; then
	exit 1
fi

count=0
while IFS=/ read -r beforeslash afterslash_1 afterslash_2 afterslash_3 afterslash_4
do
	name[$count]="$afterslash_1"
	#echo "${name[$count]}"" ""$count"
	subname[$count]="$afterslash_2"
	subnameisfile[$count]="$afterslash_3"
	level[$count]="$afterslash_4"
	hassamedir[$count]=0
	count=$(($count + 1))
done < "$memtemp""/""$param1"

total=$count

#get sub dir in remote dir 
cd "$param2"/

count=0
for pathname in ./* ; do
	if [[ -d "$pathname" ]] ; then 
		name_remote[$count]=$(basename "$pathname")
		#echo "${name_remote[$count]}"" ""$count"
		hassamedir_remote[$count]=0
		maxsavelinkedname_remote[$count]=""
		maxsavelinkedid_remote[$count]=""
		maxsubcount_remote[$count]=0
		count=$(($count + 1))
	fi
done

cd "$workingdir"/
rm "$memtemp"/"$param3"
touch "$memtemp"/"$param3"

#tim dir cung ten
for i in "${!name[@]}" ; do
	if [[ "${subname[$i]}" == "b" ]] ; then
		for j in "${!name_remote[@]}" ; do
			if [[ "${name[$i]}" == "${name_remote[$j]}" ]] ; then
				hassamedir_remote[$j]=1
				hassamedir[$i]=1
				#sau phai in ra file, dang test nen in ra man hinh
				printf "./%s/0/%s\n" "${name[$i]}" "${name_remote[$j]}" >> "$memtemp"/"$param3"
			fi
		done
	fi
done

#in ra
#echo '--------in ra-----------'
#for i in "${!name[@]}" ; do
#	if [ "${subname[$i]}" == "b" ] ; then
		
#		echo '--------next dir--------'
#		count=0
#		for (( loopforcount=$(($i + 1)); loopforcount<$total; loopforcount+=1 )); do
#			if [ "${subname[$loopforcount]}" == "e" ] ; then
#				break
#			fi
#			tempsubname[$count]="${name[$loopforcount]}"
#			tempsubnameisfile[$count]="${subnameisfile[$loopforcount]}"
#			count=$(($count + 1))
#		done
		
#		for j in "${!tempsubname[@]}" ; do
#			printf "%s %s\n" "${tempsubname[$j]}" "${tempsubnameisfile[$j]}" 
#		done
		
#		unset tempsubname
#		unset tempsubnameisfile
#	fi
#done

#echo '--------tim thu muc doi ten--------'
#tim dir co 98% noi dung trung
#lay 3 p/tu dau tien -> chay lenh find trong cac subdir phia remote co hassamedir=0
for i in "${!name[@]}" ; do
	if [[ ${hassamedir[$i]} -eq 0 ]] && [[ "${subname[$i]}" == "b" ]] ; then
		
		count=0
		for (( loopforcount=$(($i + 1)); loopforcount<$total; loopforcount+=1 )); do
			if [[ "${subname[$loopforcount]}" == "e" ]] ; then
				break
			fi
			tempsubname[$count]="${name[$loopforcount]}"
			tempsubnameisfile[$count]="${subnameisfile[$loopforcount]}"
			count=$(($count + 1))
		done
		
		max_subcount=0
		j_found=0
		for j in "${!name_remote[@]}" ; do
			if [[ ${hassamedir_remote[$j]} -eq 0 ]] ; then
				subcount=0
				count=0
				for k in "${!tempsubname[@]}" ; do
					if [[ "${tempsubnameisfile[$k]}" == "f" ]] ; then
						rs[$count]=$(find "$param2"/"${name_remote[$j]}" -maxdepth 1 -type f -name "${tempsubname[$k]}")
					else
						rs[$count]=$(find "$param2"/"${name_remote[$j]}" -maxdepth 1 -type d -name "${tempsubname[$k]}")
					fi
					if [[ "${rs[$count]}" ]] ; then
						subcount=$(( $subcount + 1 ))
					fi
					count=$(( $count + 1 ))
				done
				if [[ $subcount -gt $max_subcount ]] ; then
					max_subcount=$subcount
					#echo "$max_subcount"
					j_found=$j
					if [[ $max_subcount -eq 5 ]] ; then
						#hassamedir_remote[$j]=1
						#hassamedir[$i]=1
						#printf "landau:%s/2/%s\n" "${name[$i]}" "${name_remote[$j]}"
						if [[ $max_subcount -gt ${maxsubcount_remote[$j]} ]] ; then
							maxsavelinkedname_remote[$j]="${name[$i]}"
							maxsavelinkedid_remote[$j]="$i"
							maxsubcount_remote[$j]="$max_subcount"
						fi
						break
					fi
				fi
				
				unset rs
			fi
		done
		
		if [[ $max_subcount -gt 0 ]] && [[ $max_subcount -lt 5 ]] ; then
			#hassamedir_remote[$j_found]=1
			#hassamedir[$i]=1
			#printf "landau:%s/3/%s\n" "${name[$i]}" "${name_remote[$j_found]}"
			if [[ $max_subcount -gt ${maxsubcount_remote[$j_found]} ]] ; then
				maxsavelinkedname_remote[$j_found]="${name[$i]}"
				maxsavelinkedid_remote[$j_found]="$i"
				maxsubcount_remote[$j_found]="$max_subcount"
			fi
		fi
		
		unset tempsubname
		unset tempsubnameisfile
	fi	
done

tempid=0
for j in "${!name_remote[@]}" ; do
	if [[ "${maxsavelinkedname_remote[$j]}" != "" ]] ; then
		#printf "%s tren remote co linked toi /%s/ id= %s voi subcount=%d\n" "${name_remote[$j]}" "${maxsavelinkedname_remote[$j]}" "${maxsavelinkedid_remote[$j]}" "${maxsubcount_remote[$j]}"
		tempid="${maxsavelinkedid_remote[$j]}"
		mv "$param2"/"${name_remote[$j]}" "$param2"/"${name[$tempid]}"
		printf "./%s/2/%s\n" "${name[$tempid]}" "${name_remote[$j]}" >> "$memtemp"/"$param3"
		hassamedir_remote[$j]=1
		hassamedir["$tempid"]=1
	fi
done

#lam lai lan cuoi
for i in "${!name[@]}" ; do
	if [[ ${hassamedir[$i]} -eq 0 ]] && [[ "${subname[$i]}" == "b" ]] ; then
		
		count=0
		for (( loopforcount=$(($i + 1)); loopforcount<$total; loopforcount+=1 )); do
			if [[ "${subname[$loopforcount]}" == "e" ]] ; then
				break
			fi
			tempsubname[$count]="${name[$loopforcount]}"
			tempsubnameisfile[$count]="${subnameisfile[$loopforcount]}"
			count=$(($count + 1))
		done
		
		max_subcount=0
		j_found=0
		for j in "${!name_remote[@]}" ; do
			if [[ ${hassamedir_remote[$j]} -eq 0 ]] ; then
				subcount=0
				count=0
				for k in "${!tempsubname[@]}" ; do
					if [[ "${tempsubnameisfile[$k]}" == "f" ]] ; then
						rs[$count]=$(find "$param2"/"${name_remote[$j]}" -maxdepth 1 -type f -name "${tempsubname[$k]}")
					else
						rs[$count]=$(find "$param2"/"${name_remote[$j]}" -maxdepth 1 -type d -name "${tempsubname[$k]}")
					fi
					if [[ "${rs[$count]}" ]] ; then
						subcount=$(( $subcount + 1 ))
					fi
					count=$(( $count + 1 ))
				done
				if [[ $subcount -gt $max_subcount ]] ; then
					max_subcount=$subcount
					j_found=$j
					if [[ $max_subcount -eq 5 ]] ; then
						hassamedir_remote[$j]=1
						hassamedir[$i]=1
						#sau phai in ra file, dang test nen in ra man hinh
						printf "./%s/3/%s\n" "${name[$i]}" "${name_remote[$j]}" >> "$memtemp"/"$param3"
						mv "$param2"/"${name_remote[$j]}" "$param2"/"${name[$i]}"
						break
					fi
				fi
				
				unset rs
			fi
		done
		
		if [[ $max_subcount -gt 0 ]] && [[ $max_subcount -lt 5 ]] ; then
			hassamedir_remote[$j_found]=1
			hassamedir[$i]=1
			#sau phai in ra file, dang test nen in ra man hinh
			mv "$param2"/"${name_remote[$j_found]}" "$param2"/"${name[$i]}"
			printf "./%s/4/%s\n" "${name[$i]}" "${name_remote[$j_found]}" >> "$memtemp"/"$param3"
			
		fi
		
		unset tempsubname
		unset tempsubnameisfile
	fi	
done

#echo '--------------------------------xoa--------------------------------------'
for j in "${!name_remote[@]}" ; do
	if [[ ${hassamedir_remote[$j]} -eq 0 ]] ; then
		#printf "xoa:%s\n" "${name_remote[$j]}"
		printf "./%s/5/null\n" "${name_remote[$j]}" >> "$memtemp"/"$param3"
		rm -r "$param2"/"${name_remote[$j]}"
	fi
done

#tao dir ko co tren remote
for i in "${!name[@]}" ; do
	if [[ "${subname[$i]}" == "b" ]] ; then
			if [[ ${hassamedir[$i]} -eq 0 ]] ; then
				#sau phai in ra file, dang test nen in ra man hinh
				mkdir "$param2"/"${name[$i]}"
				printf "./%s/1/%s\n" "${name[$i]}" "${name[$i]}" >> "$memtemp"/"$param3"
			fi
	fi
done

echo './' >> "$memtemp""/""$param3"
