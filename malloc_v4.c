//block splitting

#include<unistd.h>
#include<stddef.h>

/**
* struct block_meta - metadata header for each allocated block 
* @size: size of user data reigion in bytes
* @is_free: nonzero if free, 0 if allocated 
* @next: pointer to the next metadata block in the list
 */
struct block_meta
{
	size_t size;
	int is_free;
	struct block_meta *next;
} __attribute__((aligned(16)));

static struct block_meta *head = NULL;
static struct block_meta *tail = NULL;



/**
 * find_free_block - finds a free block large enough for allocation 
 * @size: size of user data
 * 
 * Performs a first-fit search over the block list starting at @head and 
 * returns the first free block whose size is greater than or equal to @size
 *
 * Return:
 * Pointer to a suitable free block on success, or NULL if no such block exists
*/
static struct block_meta* find_free_block(size_t size)
{
        //search if free block of enough size is available to reuse -- first fit
        struct block_meta *curr = head;
	while(curr != NULL)
        {
 		if(curr->is_free==1 && curr->size >= size)
                {
                       return curr;
                }
                curr = curr->next;
        }
	return NULL;
}


/**
 * split_block - splits a block into required size 
 * @block - pointer to free block header
 * @size - required size
 */

static void split_block(struct block_meta *block, size_t size)
{
	//block splitting
        // |      free     |   ---->  |   size   |   new   |

        struct block_meta *remainder = (struct block_meta*)((char*)(block + 1) + size);
        // assigning meta data to new block
        remainder->size = block->size - size - sizeof(struct block_meta);
        remainder->is_free = 1;
        remainder->next = block->next;

        //editing free block's meta data
        block->size = size;
        block->next = remainder;

	if(block == tail) 
		tail = remainder;
}



/**
 * malloc - allocates heap memory 
 * @size: memory in bytes to be allocated
 *
 * Allocates memory block of at least @size bytes. If free block is available,
 * then the allocator reuses that, else the heap is extended using sbrk() and a new 
 * block is created.
 *
 * Returns: 
 * Pointer to the start of the memory block 
*/

void* malloc(size_t size)
{
	//search if free block of enough size is available to reuse -- first fit 
	struct block_meta *free_block = find_free_block(size);
	if(free_block)
	{
	
		free_block->is_free = 0;
		if(free_block->size == size)	
			return (void*)(free_block+1); 
	
		else if(free_block->size >= size + sizeof(struct block_meta) + 16)
		{
			split_block(free_block, size);
			return (void*)((char*)(free_block+1));
		}	
	}


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






/**
 *free
 *@ptr: pointer to memory location to be freed
 *returns: void
*/
void free(void *ptr)
{
	if(ptr == NULL) return;

	//ptr points to the user data
	//find the meta block and set is_free=1
	struct block_meta *meta = (struct block_meta*)ptr - 1;
	meta->is_free=1;	
}


