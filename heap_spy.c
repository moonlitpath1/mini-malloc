#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>

int main()
{
	printf("Heap Pointer locataion:\nBefore:%p", sbrk(0));
	malloc(1000);

	printf("\nAfter:%p", sbrk(0));
	
	
	
	return 0;

}
