client_by_tox_protocol: client_by_tox_protocol.c
	gcc -Wall -g -O0 -D_FILE_OFFSET_BITS=64 -o client_by_tox_protocol client_by_tox_protocol.c client_by_tox_protocol_file_transfers.c md5.c -ltoxcore
clean:
	-rm -f client_by_tox_protocol
