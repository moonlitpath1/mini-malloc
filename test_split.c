#include <stdio.h>
#include <stdlib.h>

int main() {
    void *a = malloc(200);  
    free(a);               

    void *b = malloc(64);   

    return 0;
}

