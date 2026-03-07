## Clues
**The one insight for Phase 4:** when `find_free_block` returns a block of say 200 bytes but you only need 64 — instead of handing over all 200, **cut it in two**:

```
BEFORE split:
[meta: size=200, free=1 | ....200 bytes..........]

AFTER split:
[meta: size=64, free=0 | ..64 bytes..][meta: size=104, free=1 | ...104 bytes...]
```

**Clue 1:** The second block's meta header goes at: `(char*)(block + 1) + size` — right after the user data of the first block. What type do you cast that to?

**Clue 2:** After splitting, the first block's `next` should point to the new second block, and the second block's `next` should point to what the first block's `next` was originally.

**Clue 3:** Only split if there's enough room — you need at least `size + sizeof(struct block_meta) + 1` bytes in the free block, otherwise there's no point.
Go! 🔨

---
## My understanding of the clue and planning

![[../images/Pasted image 20260303121829.png]]

![[../images/Pasted image 20260303111007.png]]


----



## Version 1
First, I wrote the logic directly in the `malloc()` function.
```
if (free_block->size == size)
    return (void *)(free_block + 1); 👾👾//hadn't changed value of is_free
else
{
    // block splitting

    struct block_meta *new =
        (struct block_meta *)((char *)(free_block + 1) + size);

    // assigning meta data to new block
    new->size = free_block->size - size;  👾👾//forgot to consider size of meta block (look in diagram)
    new->is_free = 1; 
    new->next = free_block->next;

    // editing free block's meta data
    free_block->size = size;
    free_block->is_free = 0;
    free_block->next = new;

    return (void *)((char *)(free_block + 1));
}
```

**Issues here:** 
1. `malloc()` became very complex.  -- i should create a helper function for it
2. bugs are highlighted in the code by 👾👾

**Extra:**
I reffered to the kernel coding style and learnt this rule:
>The maximum length of a function is inversely proportional to its complexity. Use helper functions with descriptive names when a less-than-gifted first-year student might not understand what the function is about.

This means that my malloc() function which is complex should be short and crisp. helper functions should be used by me to make my code more readable. 

---

## Version 2

Created a new helper function `split_block()` and decided to add the `static` keyword to my helper functions, to ensure they are private to my file.

```
static void split_block(struct block_meta *block, size_t size)
{
	//block splitting
        // |      free     |   ---->  |   size   |   new   |

        struct block_meta *remainder = (struct block_meta*)((char*)(block + 1) + size);
        // assigning meta data to new block
        remainder->size = block->size - size - sizeof(struct block_meta);   👾👾//BUG
        remainder->is_free = 1;
        remainder->next = block->next;

        //editing free block's meta data
        block->size = size;
        block->next = remainder;

	if(block == tail) 
	{
	        tail->next = remainder;	
		tail = remainder;
	}
}

```

```
void* malloc(size_t size)
{
	//search if free block of enough size is available to reuse -- first fit 
	struct block_meta *free_block = find_free_block(size);
	if(free_block)
	{
	
		free_block->is_free = 0;
		if(free_block->size == size)	
			return (void*)(free_block+1); 	
	
		else
		{
			split_block(free_block, size);
			return (void*)((char*)(free_block+1));
		}	
	}
	
	.
	.
	.
}

```

This code has a major BUG that i had failed to notice:
**`size_t` Underflow**

```
remainder->size = block->size - size - sizeof(struct block_meta);
```

size_t is unsigned. this means that if the result is negative.. it underflows. 

**for example:**

	block->size = 65
	size        = 64
	sizeof(meta)= 32

	65 - 64 - 32 = -31   ← mathematically
	But since size_t is unassigned, -31 wraps to 18446744073709551585  which is huge! 
	remainder->size is set to that! and so my allocator tries to write that to memory 👾👾

So, it is crucial to check if there's enough room to split before calling split_block.

```
	if(free_block)
	{
	
		free_block->is_free = 0;
		if(free_block->size == size)	
			return (void*)(free_block+1); 	
	
		else if(free_block->size >= size + sizeof(struct block_meta) + 1)
		{
			split_block(free_block, size);
			return (void*)((char*)(free_block+1));
		}	
	}

```

this fixed the segmentation fault. 

---
## Full code:
```
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
	
		else if(free_block->size >= size + sizeof(struct block_meta) + 1)
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

```



---
i tested it with this code
```

```
 and it works. 
```
 anu@laptop:~/anu/programming/systems_progg/mini-malloc$ LD_PRELOAD=./malloc_v4.so strace -e trace=brk ./out 2>&1
brk(NULL)                               = 0x555555559000
brk(NULL)                               = 0x555555559000
brk(0x5555555590e8)                     = 0x5555555590e8
+++ exited with 0 +++

```

