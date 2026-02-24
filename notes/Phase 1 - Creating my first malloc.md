### Clues that I've got: 

Create `mymalloc.c`. You need **two** functions (not one):

```
#include <unistd.h>
#include <stddef.h>

void *malloc(size_t size) {
    // your 3-line bump allocator here
}

void free(void *ptr) {
    // intentionally empty for now — but must exist
}
```

**Why does `free` need to exist even empty?** When `ls` runs, it calls `free()` on memory it allocated. If you don't provide `free`, your `.so` has no `free` symbol, so the linker falls back to glibc's `free` — which will try to read metadata from a pointer your malloc never wrote, and **segfault.** A stub that does nothing is safer for now.

**Your 3 clues for `malloc`:**
1. `sbrk(0)` → returns current heap end → **this is what you return to the caller**
2. `sbrk(size)` → shoves the heap forward by `size` bytes
3. Do step 1 first, step 2 second — one line each

**Failure check clue:** `sbrk()` returns `(void *)-1` on failure. Real malloc returns `NULL` on failure. Handle it.

**Compile + inject:**
```bash
gcc -shared -fPIC -o mymalloc.so mymalloc.c
LD_PRELOAD=./mymalloc.so ls
```

**What success looks like:** `ls` prints your files. No segfault.

**What to watch for:** Run it with `strace -e trace=brk` too. You'll see something very different from glibc's single big-grab behavior — your allocator hits `brk` on every single call. That's the inefficiency you're building toward fixing in Phase 3.

---

**Mistake No. 1**
I created a main function and tried running a test code in it, while creating the malloc and free functions and leaving them empty. 

However, this lead to segfault. 

**Reason**: 
My code's `malloc()` was overriding the original malloc's and free's code. 
And `printf()` depends upon `malloc()` to create space since it uses the output buffer, etc. 
So, when it encountered the empty malloc() it ran into a frenzy and this lead to a segfault. 

```
#include<stdio.h>
#include<unistd.h>
#include<stddef.h>

void* malloc(size_t size)
{
    /* void *top = sbrk(0); */
    /* printf("Print %p\n", top); */
}

void free(void *ptr)
{
    /*
    intentionally empty for now
    */
}

int main()
{
    printf("%p\n", sbrk(0));
    return 0;
}
```

I learnt that the main function is not needed here, and to never use printfs inside an malloc. 

