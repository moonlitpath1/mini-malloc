#include<stdio.h>
#include<stdlib.h>

void heap_dump(void);

int main()
{
	printf(" ");
	void *ptr = malloc(128 * 1024); 
	heap_dump();	
	void *ptr2 = malloc(1);
	heap_dump();
	free(ptr);
	free(ptr2);
	return 0;
}
