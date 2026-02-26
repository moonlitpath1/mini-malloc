#include <stdio.h>
#include <stdlib.h>

int main() {
    void *ptrs[10];

    // allocating 10 blocks of 32 bytes
    for(int i = 0; i < 10; i++)
        ptrs[i] = malloc(32);

    // free only the even-indexed ones
    for(int i = 0; i < 10; i += 2)
        free(ptrs[i]);

	//allocate 64 bytes... will it create new block or reuse??
    void *big = malloc(64);

    return 0;
}
