#include <stdio.h>
#include <stdlib.h>




char *hash(FILE *f) {
    int block_size = 8;
    char *hash_val = malloc(block_size);
    char ch;
    int hash_index = 0;

    for (int index = 0; index < block_size; index++) {
        hash_val[index] = '\0';
    }

    while(fread(&ch, 1, 1, f) != 0) {
        hash_val[hash_index] ^= ch;
        hash_index = (hash_index + 1) % block_size;
    }

    return hash_val;
}
