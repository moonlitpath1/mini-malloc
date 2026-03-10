For large allocations, `mmap` is better than `sbrk` since while freeing, `mmap` block with `munmap` returns memory back to the kernel unlike `sbrk` blocks which stay in your heap forever. 

To observe this in person, I created a simple `test_threshold.c` code that i traced with `strace`
```
#include<stdlib.h>
int main()
{
	void *ptr = malloc(256 * 1024); //256kb
	free(ptr);
	return 0;
}

```

**Result:**
```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ gcc test_threshold.c -o out && strace -e trace=brk,mmap ./out 2>&1 | grep -E "brk|mmap"

--------- library loading -------

brk(NULL)                               = 0x555555559000
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7ffff7fbb000
mmap(NULL, 81463, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7ffff7fa7000
mmap(NULL, 2170256, PROT_READ, MAP_PRIVATE|MAP_DENYWRITE, 3, 0) = 0x7ffff7c00000
mmap(0x7ffff7c28000, 1605632, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x28000) = 0x7ffff7c28000
mmap(0x7ffff7db0000, 323584, PROT_READ, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1b0000) = 0x7ffff7db0000
mmap(0x7ffff7dff000, 24576, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 3, 0x1fe000) = 0x7ffff7dff000
mmap(0x7ffff7e05000, 52624, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_ANONYMOUS, -1, 0) = 0x7ffff7e05000
mmap(NULL, 12288, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7ffff7fa4000

---- glibc heap setup
brk(NULL)                               = 0x555555559000
brk(0x55555557a000)                     = 0x55555557a000



-----my allocation starts from here -----------

mmap(NULL, 266240, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7ffff7f63000

```

mmap maps appx 260KB memory, the extra 4KB is due to page size rounding. The OS only gives memory in 4KB page chunks.

----
## The Threshold on the System

glibc's default `MMAP_THRESHOLD` starts at **128KB** and is **dynamic** — it adjusts upward based on the largest freed mmap chunk.

```
#include <malloc.h>
#include <stdlib.h>

int main()
{
    void *p1 = malloc(64 * 1024);   // 64 KB
    malloc_stats();

    void *p2 = malloc(256 * 1024);  // 256 KB
    malloc_stats();

    free(p1);
    free(p2);
}

```


Output:
```
Arena 0:
system bytes     =     135168
in use bytes     =      66208
Total (incl. mmap):
system bytes     =     135168
in use bytes     =      66208
max mmap regions =          0
max mmap bytes   =          0

Arena 0:
system bytes     =     135168
in use bytes     =      66208
Total (incl. mmap):
system bytes     =     401408
in use bytes     =     332448
max mmap regions =          1
max mmap bytes   =     266240

```

In the first `malloc(64)`
Glibc requested for 132 KB, and allocated 66KB( extra allocator metadata) using malloc. 
Since the requested allocation size is lesser than threshold
`64 < 128`
thus, we can see that mmap reigions are 0

In the second `malloc(256)`
Glibc uses mmap for allocation here, since the value is above threshold. 
`256 > 128`

---

## The concept:
>For large allocations (above a threshold), `sbrk` is wasteful. `mmap` is better because freeing an `mmap` block with `munmap` actually returns memory to the OS — unlike `sbrk` blocks which stay in your heap forever.

---

## **Three clues for the implementation:**

1. If `size >= MMAP_THRESHOLD`, call `mmap(NULL, size + sizeof(struct block_meta), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)` instead of `sbrk`. This asks the kernel to create a **new anonymous private memory mapping** of the requested size (plus metadata) with **read/write permissions**, placed at an address chosen by the kernel and **not backed by any file**. The mapping is separate from the heap and is typically page-aligned.

2. mmap'd blocks should NOT be added to your free list — they live outside the heap and get returned directly to the OS via `munmap` in `free()`

3. How does `free()` know if a pointer came from mmap vs sbrk? **Clue:** add a field to `block_meta` — `int is_mmap` — and check it in `free()`


---


## Implementation


#### Malloc allocates when size > 128 KB

```
#define MMAP_THRESHOLD (128*1024)
```

```
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

```



#### Free function frees block using munmap if it was allocated with mmap

```
struct block_meta
{
	size_t size;
	int is_free;
	int is_mmap;
	struct block_meta *next;
} __attribute__((aligned(16)));

```

```
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

```


---

### Testing program

Created simple program: `test_threshold.c`
```
int main()
{
    void *ptr = malloc(128 * 1024);
    heap_dump();

    void *ptr2 = malloc(1);
    heap_dump();

    free(ptr);
    free(ptr2);

    return 0;
}
```

Output:
```
Block 1: | Free: 0 | Size: 131072 | MMAP: 1
Block 2: | Free: 0 | Size: 1024 | MMAP: 0

Block 1: | Free: 0 | Size: 131072 | MMAP: 1
Block 2: | Free: 0 | Size: 1024 | MMAP: 0
Block 3: | Free: 0 | Size: 1 | MMAP: 0
```

During my first allocation, 2 blocks were allocated. 
The first block is from my program. I will explain Block2 later. 

As expected,
Block 1, due to it's big size was allocated by mmap()
and Block 3, due to it's small size was allocated using sbrk()

The second block is a mystery though...
The second block was probably formed because of `printf()` that was used in my `heap_dump()` function creating a 1024 byte 1024 buffer. 

To verify this, I added a printf(" ") statement before calling malloc to see if it becomes the first block. 

and as expected:
```
 Block 1: | Free: 0 | Size: 1024 | MMAP: 0
Block 2: | Free: 0 | Size: 131072 | MMAP: 1

Block 1: | Free: 0 | Size: 1024 | MMAP: 0
Block 2: | Free: 0 | Size: 131072 | MMAP: 1
Block 3: | Free: 0 | Size: 1 | MMAP: 0
```

The first block is 1024 bytes now, which was allocated by printf's buffer. 


---

## Complete code

```
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

```
