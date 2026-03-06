#include<stdio.h>
#include<stdlib.h>
void heap_dump(void);
int main()
{
	void *a, *b, *c, *d;
	a = malloc(32);
	b = malloc(64);
	c = malloc(64);	

	heap_dump();

	free(a);
	free(b);	

	heap_dump();

	
	return 0;
}
