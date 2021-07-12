/*
 * mm.c
 *
 * Name: [FILL IN]
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read malloclab.pdf carefully and in its entirety before beginning.
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */


// CONSTANTS
#define ALIGNMENT         8         // memory alignment factor
#define WSIZE             4         // Size in bytes of a single word 
#define DSIZE             8         // Size in bytes of a double word
#define INITSIZE          16        // Initial size of free list before first free block added
#define MINBLOCKSIZE      16        /* Minmum size for a free block, includes 4 bytes for header/footer
                                       and space within the payload for two pointers to the prev and next
                                       free blocks */

// MACROS
/* NOTE: Most of these macros came from the text book on Page 857 (Fig. 9.43). We added the
 * NEXT_FREE and PREV_FREE macros to traverse the free list */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define MAX(x, y) ((x) > (y)? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p)        (*(size_t *)(p))
#define PUT(p, val)   (*(size_t *)(p) = (val))
#define GET_SIZE(p)  (GET(p) & ~0x1)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp)     ((void *)(bp) - WSIZE)
#define FTRP(bp)     ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))
#define NEXT_FREE(bp)(*(void **)(bp))
#define PREV_FREE(bp)(*(void **)(bp + WSIZE))


// PROTOTYPES
static void *extend_heap(size_t words);
static void *find_fit(size_t size);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void remove_freeblock(void *bp);
// static int mm_check();


// Private variables represeneting the heap and free list within the heap
static char *heap_listp = 0;  /* Points to the start of the heap */
static char *free_listp = 0;  /* Poitns to the frist free block */

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{// Initialize the heap with freelist prologue/epilogoue and space for the
  // initial free block. (32 bytes total)
  if ((heap_listp = mem_sbrk(INITSIZE + MINBLOCKSIZE)) == (void *)-1)
      return false; 
  PUT(heap_listp,             PACK(MINBLOCKSIZE, 1));           // Prologue header 
  PUT(heap_listp +    WSIZE,  PACK(MINBLOCKSIZE, 0));           // Free block header 

  PUT(heap_listp + (2*WSIZE), PACK(0,0));                       // Space for next pointer 
  PUT(heap_listp + (3*WSIZE), PACK(0,0));                       // Space for prev pointer 
  
  PUT(heap_listp + (4*WSIZE), PACK(MINBLOCKSIZE, 0));           // Free block footer 
  PUT(heap_listp + (5*WSIZE), PACK(0, 1));                      // Epilogue header 

  // Point free_list to the first header of the first free block
  free_listp = heap_listp + (WSIZE);

  return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    /* IMPLEMENT THIS */
    if (size == 0)
      return NULL;

  size_t asize;       // Adjusted block size 
  size_t extendsize;  // Amount to extend heap by if no fit 
  char *bp;

  /* The size of the new block is equal to the size of the header and footer, plus
   * the size of the payload. Or MINBLOCKSIZE if the requested size is smaller.
   */
  asize = MAX(ALIGN(size) + DSIZE, MINBLOCKSIZE);
  
  // Search the free list for the fit 
  if ((bp = find_fit(asize))) {
    place(bp, asize);
    return bp;
  }

  // Otherwise, no fit was found. Grow the heap larger. 
  extendsize = MAX(asize, MINBLOCKSIZE);
  if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;

  // Place the newly allocated block
  place(bp, asize);

  return bp;
}

/*
 * free
 */
void free(void* ptr)
{
   // Ignore spurious requests 
  if (!ptr)
      return;

  size_t size = GET_SIZE(HDRP(ptr));

  /* Set the header and footer allocated bits to 0, thus
   * freeing the block */
  PUT(HDRP(ptr), PACK(size, 0));
  PUT(FTRP(ptr), PACK(size, 0));

  // Coalesce to merge any free blocks and add them to the list 
  coalesce(ptr);
}

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    /* IMPLEMENT THIS */
    return NULL;
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
#endif /* DEBUG */
    return true;
}
