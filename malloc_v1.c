//the most basic malloc ever
//aka bump allocator -- since the ptr can only move forward like a bump on a counter --- your heap can only grow. never shrink. 
//no memory reusability. 


#include<unistd.h>
#include<stddef.h>

void* malloc(size_t size)
{
	void *top = sbrk(0);
	if(sbrk(size) == (void *)-1) return NULL;
	return top;
}

void free(void *ptr)
{
	(void)ptr;
	//intentionally empty for now -- we are going to test it on `ls` 
	//but ls calls free() on memory, 
	//if we dont provide the function the .so file has no free symbol,
	//so the linkter falls back to glibc's free
	//which tries to read metadata from a pointer malloc never wrote
	//and segfault. 
}


