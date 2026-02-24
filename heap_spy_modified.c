#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>

int main()
{
	printf("Heap Pointer locataion:\nBefore:%p", sbrk(0));
	
	printf("\n1st malloc\nMalloc Pointer Address: %p\n", malloc(1000));
	printf("Heap Pointer location:%p", sbrk(0));

	printf("\n2nd malloc\nMalloc Pointer Address: %p\n", malloc(1000));
	printf("Heap Pointer location:%p", sbrk(0));

	printf("\n3rd malloc\nMalloc Pointer Address: %p\n", malloc(1000));
	printf("Heap Pointer location:%p", sbrk(0));
	char cmd[64]; //creates a character buffer
	
	//proc -- linux virtual file system -- you are getting memory mappings of a process that containt the word "heap"
	
	sprintf(cmd, "cat /proc/%d/maps | grep heap", getpid()); 
	system(cmd); //executes the command using /bin/sh

	
	return 0;

}
