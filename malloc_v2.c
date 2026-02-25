// in v1 we returned raw memory and 0 metadata 
// v2 will store a small header before user data -- this will help free()

#include<unistd.h>
#include<stddef.h>

struct block_meta
{
	size_t size;
	int is_free;
	struct block_meta *next;
} __attribute__((aligned(16)));

struct block_meta *head = NULL;
struct block_meta *tail = NULL;

void* malloc(size_t size)
{
	//getting top of the heap
	void *top = sbrk(0);
	
	//allocting space for user data and meta data
	if( sbrk(size + sizeof(struct block_meta) ) == (void *)-1 ) return NULL;

	//meta data struct would start at the heap top
	struct block_meta *meta_data = (struct block_meta*)top;
	meta_data->size = size;
	meta_data->is_free=0;
	meta_data->next=NULL;

	//adding to metadata linkedlist	
	if(tail == NULL)
	{
		head = meta_data;
		tail=meta_data;
	}
	else
	{
		tail->next = meta_data;
		tail=meta_data;
	}

	//userdata memory starts after metadata
	top = (char*)top+sizeof(*meta_data);
	return top;
}

void free(void *ptr)
{
	(void)ptr;
	//intentionally empty for now -- we are going to test it on `ls` 
}


