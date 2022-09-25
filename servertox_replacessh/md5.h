#ifndef MD5_H
#define MD5_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

void print_hash(uint8_t *p);

/* input: out must be allocated with 16 bytes */
void md5hash(char * filename,uint64_t sz, char *out);


#endif /* CLIENTSYNCTOX_FILE_TRANSFERS_H */
