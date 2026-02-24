//the most basic malloc ever

#include<unistd.h>
#include<stddef.h>

void* malloc(size_t size)
{
//	void *top = sbrk(0);
//	printf("Print%p", top);
}

void free(void *ptr)
{
	//intentionally empty for now -- we are going to test it on `ls` 
	//but ls calls free() on memory, 
	//if we dont provide the function the .so file has no free symbol,
	//so the linkter falls back to glibc's free
	//which tries to read metadata from a pointer malloc never wrote
	//and segfault. 
}


int main()
{

	return 0;
}
