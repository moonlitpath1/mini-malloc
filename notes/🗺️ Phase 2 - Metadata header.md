## **Clues**

The jump from v1 to v2 is **one new concept**: a `struct` hidden just before the pointer you return.

Your v1 returned raw memory with zero metadata. v2 will secretly store a small header _before_ the user's data. That header is what will eventually let `free()` do real work.

Here's the shape of the struct you need to define — fill in the field types yourself:
```
struct block_meta {
    _______ size;      // how big is this block?
    _______ is_free;   // is this block available to reuse?
    _______ *next;     // pointer to the next block in the list
};
```

**The key question to think about:** if this struct sits _before_ the user data in memory, and your `malloc()` needs to return the address of the user data (not the struct) — what arithmetic do you do on the pointer that `sbrk()` gives you before returning it?

---

## **What I learnt:**

Every time `malloc(N)` is called, you carve this out of the heap
```
  ┌─────────────────────┬──────────────────────────────┐
  │     block_meta      │        user data             │
  │  size | free | next │  ← N bytes the caller uses   │
  └─────────────────────┴──────────────────────────────┘
  ^                     ^
  sbrk() gives you      THIS is what malloc() returns
  this address          (this is what you called *top)

```


**Use of `*next`:**
Each meta-block's `*next` points to the next meta-block's `*next`

Aka, if you call 3 malloc()s in your code, 
A linked list of 3 nodes would be formed by the meta-blocks. 

This is used to reuse memory. For example, if you have freed a block of 80 bytes, then it can be reused by the next code. Just check if is_free for each node. 

you need 1 global variable here that will act as the head of the LL. 
`struct block_meta *global_base = NULL;`


----

## **My first attempt:**
```
// in v1 we returned raw memory and 0 metadata 
// v2 will store a small header before user data -- this will help free()

#include<unistd.h>
#include<stddef.h>

struct block_meta
{
	size_t size;
	int is_free;
	struct block_meta *next;
};

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

```

It ran successfully!
```
anu@laptop:~/anu/programming/systems_progg/mini-malloc$ gcc -shared -fPIC -o malloc_v2.so malloc_v2.c && LD_PRELOAD=./malloc_v2.so ls
faulty_heap_spy.c  heap_spy_modified.c  malloc_v1.so  malloc_v2.so  og_malloc.c
heap_spy.c         malloc_v1.c          malloc_v2.c   notes         out
```



**Issue: The Alignment trap**
The `struct block_meta` occupies 24 bytes.  

The CPU reads in fixed sized chunks, typically 64 bytes at a time, and within those chunks, it wants data to start at clean boundaries. 
For example, 
you have a `double` (8 bytes) that starts at 0 will be read cleanly
```
slot 0    slot 8    slot 16   slot 24
[........][........][........][........]
```
however, if the `double` starts at 5 bytes, it will span 2 slots, needing 2 reads. 
This is slow and may cause crashes.

Malloc() must be stable and return memory addresses for any type of variable. The maximum size of a var in C is 16 bytes (max_align_t).
So, malloc must return 16 byte aligned addresses. 

My current code does not do that.

to fix it, 
```
struct block_meta 
{
    size_t size;
    int is_free;
    struct block_meta *next;
} __attribute__((aligned(16)));

```
this aligns the chunks properly. 
and now, the size of the `struct` is 32 bytes. 