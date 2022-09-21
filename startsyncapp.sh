#!/bin/bash

#Windows forbidden character of name:  <  >  :  "  /  \  |  ?  * 
        
shopt -s dotglob
shopt -s nullglob

cd /home/dungnt/Backup/Store/Projects/SyncDirApp
python3 perdatastorepro_v2.py 
#/usr/bin/gnome-terminal
