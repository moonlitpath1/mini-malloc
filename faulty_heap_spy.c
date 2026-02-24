#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main()
{
    printf("Heap Pointer location:\nBefore:%p", sbrk(0));
    printf("\n1st malloc\nMalloc Pointer Address: %p\nHeap Pointer location:%p", malloc(1000), sbrk(0));
    printf("\n2nd malloc\nMalloc Pointer Address: %p\nHeap Pointer location:%p", malloc(1000), sbrk(0));
    printf("\n3rd malloc\nMalloc Pointer Address: %p\nHeap Pointer location:%p", malloc(1000), sbrk(0));

    return 0;
}
