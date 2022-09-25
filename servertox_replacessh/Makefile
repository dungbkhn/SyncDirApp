server_by_tox_protocol: server_by_tox_protocol.c
	gcc -Wall -g -O0 -D_FILE_OFFSET_BITS=64 -o server_by_tox_protocol server_by_tox_protocol.c server_by_tox_protocol_file_transfer.c md5.c -ltoxcore
clean:
	-rm -f server_by_tox_protocol
