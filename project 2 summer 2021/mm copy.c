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

/* What is the correct alignment? */
#define ALIGNMENT 8

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ 
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  

static char *heap_listp = 0;  /* Points to the start of the heap */

//#define MAX(x, y) ((x) > (y)? (x) : (y))  
static size_t  MAX(size_t  x, size_t  y){
    return (x) > (y)? (x) : (y) ;
    }

/* Pack a size and allocated bit into a word */
//#define PACK(size, alloc)  ((size) | (alloc)) 

static size_t PACK(size_t size,size_t alloc){

    return ((size) | (alloc));

}


/* Read and write a word at address p */
//#define GET(p)       (*(unsigned int *)(p)) 
static size_t GET(void* p){

    return (*(unsigned int *)(p)) ; 
}

//#define PUT(p, val)  (*(unsigned int *)(p) = (val))    

static void PUT(size_t p, size_t val){

    *(unsigned int *)(p) = val;

}

/* Read the size and allocated fields from address p */
//#define GET_SIZE(p)  (GET(p) & ~0x7)

static size_t GET_SIZE(size_t p){

    return (GET(p) & ~0x7);
}

//#define GET_ALLOC(p) (GET(p) & 0x1) 

static size_t GET_ALLOC(size_t p){

    return (GET(p) & 0x1); 
}

/* Given block ptr bp, compute address of its header and footer */
//#define HDRP(bp)       ((char *)(bp) - WSIZE)
static char * HDRP(char* bp){

    return ((char *)(bp) - WSIZE); 
}

//#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

static char * FTRP(char* bp){

    return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE);
}
/* Given block ptr bp, compute address of next and previous blocks */
//#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 


static char * NEXT_BLKP(char * bp){ 

 return ((char *)(bp) + GET_SIZE(((char *)(bp) - DSIZE)));
}


static char * PREV_BLKP(char * bp){ 

 return ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)));
}


/* $end mallocmacros */

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)  
	    return NULL;                                        

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         
    PUT(FTRP(bp), PACK(size, 0));         
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 

    /* Coalesce if the previous block was free */
    return coalesce(bp);                                          
}

static void *find_fit(size_t asize)
{
/* First fit search */
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; /* No fit */
}


/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) //line:vm:mm:begininit
	    return false;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2*WSIZE);                     //line:vm:mm:endinit
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return  false;
    return true;
}

/*
 * malloc
 */
void* malloc(size_t size)
{
    /* IMPLEMENT THIS */

    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;      

    if (size == 0)
	    return NULL;

    if (size <= DSIZE)                                          
	    asize = 2*DSIZE;                                       
    else
	    asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    if ((bp = find_fit(asize)) != NULL) {  
	    place(bp, asize);                  
	    return bp;
    }

    extendsize = MAX(asize,CHUNKSIZE);                 //line:vm:mm:growheap1
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
	    return NULL;                                  //line:vm:mm:growheap2
    place(bp, asize);                                 //line:vm:mm:growheap3
    return bp;
}

/*
 * free
 */
void free(void* ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { /* Case 1 */
        return bp;
    }else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }else { /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
        GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
 }

/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{

    size_t oldsize;
    void *newptr;
    /* IMPLEMENT THIS */
    if (oldptr==NULL){

        return mm_malloc(size);
        //return NULL;
    }

    if (size==0){
        free(oldptr);
        return 0;
    }
    
    
    newptr = mm_malloc(size);

    
    oldsize = GET_SIZE(HDRP(oldptr));
    if(size < oldsize){
        oldsize = size;
    }
        
    memcpy(newptr, oldptr, oldsize);

    mm_free(oldptr);

    return newptr;
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
