#include <malloc.h>
#include <stdlib.h>

int main()
{
    void *p1 = malloc(64 * 1024);   // 64 KB
    malloc_stats();

    void *p2 = malloc(256 * 1024);  // 256 KB
    malloc_stats();

    free(p1);
    free(p2);
}
