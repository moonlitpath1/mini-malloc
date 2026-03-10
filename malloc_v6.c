//mmap
#include<stdio.h>
#include<unistd.h>
#include<stddef.h>
#include<sys/mman.h>

#define MMAP_THRESHOLD (128*1024)

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
	int is_mmap;
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
		if(curr->is_mmap) continue;
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
        // |      free     |   ---->  |   required_size   |   remainder   |

        struct block_meta *remainder = (struct block_meta*)((char*)(block + 1) + size + sizeof(size_t));
        // assigning meta data to new block
        remainder->size = block->size - size - sizeof(struct block_meta) - sizeof(size_t);
        remainder->is_free = 1;
        remainder->next = block->next;

	//editing footer
	size_t *footer = (size_t*)((char*)(remainder+1) + remainder->size);
	*footer = remainder->size;
        //editing free block's meta data
        block->size = size;
        block->next = remainder;
	footer = (size_t*)((char*)(block+1) + block->size);
	*footer = block->size;

	if(block == tail) 
		tail = remainder;
}



//foward coalescing
void forward_coalesce(struct block_meta *curr_block)
{
        //check if it's the last block in physical memory
        if((void*)((char*)(curr_block+1)+curr_block->size) >= sbrk(0)) return;
        
	struct block_meta *next_block = (struct block_meta*)((char*)(curr_block+1)+curr_block->size + sizeof(size_t));
        if(!next_block->is_free) return; 

        curr_block->size = curr_block->size + sizeof(size_t) + sizeof(struct block_meta) + next_block->size;
        curr_block->next = next_block->next;

	size_t *footer = (size_t*)((char*)(curr_block+1) + curr_block->size);
	*footer = curr_block->size;
}




//backward coalescing
void backward_coalesce(struct block_meta *curr_block)
{
	if(curr_block == head) return;

	size_t *footer = (size_t*)((char*)curr_block - sizeof(size_t));
	struct block_meta *prev_block = (struct block_meta*)((char*)footer - *footer - sizeof(struct block_meta));
	if(!prev_block->is_free) return;

	prev_block->size = prev_block->size + sizeof(size_t) + sizeof(struct block_meta) + curr_block->size;
	prev_block->next = curr_block->next;

	if(curr_block == tail) tail = prev_block;

	footer = (size_t*)((char*)(prev_block+1) + prev_block->size);
	*footer = prev_block->size;
	
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
	struct block_meta *block;
	if(block = find_free_block(size))
	{
		block->is_free = 0;
		if(block->size > sizeof(struct block_meta) + sizeof(size_t) + 16)
			split_block(block, size);
	}
	else
	{
		if(size >= MMAP_THRESHOLD)
		{
			block = mmap(NULL, size + sizeof(struct block_meta), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
			if(block == MAP_FAILED) return NULL;
			block->is_mmap = 1;
			
			return block;
		}


		block = sbrk(0);
		//allocting space for user data, meta data and footer
        	if( sbrk(size + sizeof(struct block_meta) + sizeof(size_t)) == (void *)-1 ) return NULL;
		block->is_mmap = 0;
	
		size_t *footer = (size_t*)((char*)(block+1) + size);
		*footer = size;

			
        	//meta data struct would start at the heap top
        	block->size = size;
        	block->is_free=0;
        	block->next=NULL;

        	//adding to metadata linkedlist 
        	if(tail == NULL)
        	{
                	head = block;
                	tail = block;
        	}
       		 else
    		{
       			tail->next = block;
	                tail = block;
	        }
		
	}
       
	return (void*)(block+1);

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
	struct block_meta *curr_block = (struct block_meta*)ptr - 1;
	
	if(curr_block->is_mmap == 1) 
	{
		munmap(curr_block, curr_block->size + sizeof(struct block_meta) );
		return;
	}


	curr_block->is_free=1;
	
	//coalescing
	backward_coalesce(curr_block);
	forward_coalesce(curr_block);

}


/*prints heap contents */
void heap_dump()
{
	struct block_meta *curr = head;
	int i=0;
	while(curr != NULL)
	{
		i++;
		printf("Block %d: | Free: %d | Size: %zu | MMAP: %d\n",i, curr->is_free, curr->size, curr->is_mmap); 
		curr = curr->next;
	}

}
