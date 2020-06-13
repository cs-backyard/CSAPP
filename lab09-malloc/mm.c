/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "lambdafate",
    /* First member's full name */
    "Wu",
    /* First member's email address */
    "lambdafate@163.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

#define MAX(v1, v2) (((v1) > (v2)) ? (v1) : (v2))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
// #define ALIGN(size)       (ALIGNMENT * (size + ALIGNMENT - 1) / ALIGNMENT)

#define PACK(size, alloc) ((size) | (alloc))

#define GET(hfp) (*(unsigned int *)(hfp))
#define PUT(hfp, val) (*(unsigned int *)(hfp) = (val))

#define GET_ALLOC(hfp) (GET(hfp) & 0x01)
#define GET_SIZE(hfp) (GET(hfp) & ~0x07)

// get head and foot pointer by block pointer
#define HEAD(bp) ((char *)(bp)-WSIZE)
#define FOOT(bp) ((char *)(bp) + GET_SIZE(HEAD(bp)) - DSIZE)

// get prev and next block pointer by current pointer
#define BLOCK_PREV(bp) ((char *)(bp)-GET_SIZE(((char *)bp - DSIZE)))
#define BLOCK_NEXT(bp) ((char *)(bp) + GET_SIZE(HEAD(bp)))

static char *heap_listp;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);
static void heap_checker();

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = (char *)mem_sbrk(4 * WSIZE)) == (void *)(-1))
    {
        return -1;
    }

    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += DSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }

    // printf("mm_init:\n");
    // heap_checker();
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size <= 0)
    {
        return NULL;
    }
    // printf("size: %d  ", size);
    size = (size + DSIZE + ALIGNMENT - 1) / ALIGNMENT;
    size = ALIGNMENT * size;
    // printf("malloc size: %d\t", size);
    char *bp;
    if ((bp = find_fit(size)) != NULL)
    {
        // printf("malloc ptr: %p\n", bp);
        place(bp, size);
        // heap_checker();
        return bp;
    }
    if ((bp = extend_heap(MAX(size, CHUNKSIZE) / WSIZE)) == NULL)
    {
        // printf("malloc ptr: NULL\n");
        return NULL;
    }
    place(bp, size);
    // printf("malloc ptr: %p\n", bp);
    // heap_checker();
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    // heap_checker();
    if (ptr == NULL)
    {
        return;
    }
    // printf("free ptr: %p\n", ptr);
    size_t size = GET_SIZE(HEAD(ptr));
    PUT(HEAD(ptr), PACK(size, 0));
    PUT(HEAD(ptr), PACK(size, 0));

    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        return mm_malloc(size);
    }
    if (size <= 0)
    {
        mm_free(ptr);
        return NULL;
    }

    char *bp = (char *)mm_malloc(size);
    if (bp == NULL)
    {
        mm_free(ptr);
        return NULL;
    }
    size_t copy_size = GET_SIZE(HEAD(ptr)) - DSIZE;
    if (GET_SIZE(HEAD(ptr)) > GET_SIZE(HEAD(bp)))
    {
        copy_size = GET_SIZE(HEAD(bp)) - DSIZE;
    }
    memmove(bp, ptr, copy_size);
    mm_free(ptr);
    return bp;
}

static void *extend_heap(size_t words)
{
    char *brk;
    size_t size = (words % 2) ? ((words + 1) * WSIZE) : (words * WSIZE);

    if ((brk = (char *)(mem_sbrk(size))) == (void *)-1)
    {
        return NULL;
    }

    PUT(HEAD(brk), PACK(size, 0));
    PUT(FOOT(brk), PACK(size, 0));
    PUT(HEAD(BLOCK_NEXT(brk)), PACK(0, 1));

    // coalesce
    return coalesce(brk);
}

/*
    coalesce prev and next free blocks
    case 1: prev alloced, next alloced
    case 2: prev alloced, next free
    case 3: prev free   , next alloced
    case 4: prev free   , next free
*/
static void *coalesce(void *bp)
{
    int prev_alloc = GET_ALLOC(HEAD(BLOCK_PREV(bp)));
    int next_alloc = GET_ALLOC(HEAD(BLOCK_NEXT(bp)));
    size_t size = GET_SIZE(HEAD(bp));

    if (prev_alloc && next_alloc)
    {
        return bp; // case 1
    }
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HEAD(BLOCK_NEXT(bp)));
        PUT(HEAD(bp), PACK(size, 0));
        PUT(FOOT(bp), PACK(size, 0));
        return bp;
    }
    else if (!prev_alloc && next_alloc)
    {
        char *prev_bp = BLOCK_PREV(bp);
        size += GET_SIZE(HEAD(prev_bp));
        PUT(HEAD(prev_bp), PACK(size, 0));
        PUT(FOOT(prev_bp), PACK(size, 0));
        return prev_bp;
    }
    else
    {
        size += GET_SIZE(HEAD(BLOCK_PREV(bp))) + GET_SIZE(HEAD(BLOCK_NEXT(bp)));
        bp = BLOCK_PREV(bp);
        PUT(HEAD(bp), PACK(size, 0));
        PUT(FOOT(bp), PACK(size, 0));
        return bp;
    }
}

static void *find_fit(size_t size)
{
    // first fit

    char *bp = BLOCK_NEXT(heap_listp);
    char *brk = mem_heap_hi() + 1;
    while (bp < brk)
    {
        if (GET_ALLOC(HEAD(bp)) == 0 && GET_SIZE(HEAD(bp)) >= size)
        {
            return bp;
        }
        bp = BLOCK_NEXT(bp);
    }
    return NULL;

    // best fit
    /*
   char *bp = BLOCK_NEXT(heap_listp);
   char *brk = mem_heap_hi() + 1;
   char *res = NULL;
   while(bp < brk){
       if(GET_ALLOC(HEAD(bp)) == 0 && GET_SIZE(HEAD(bp)) >= size){
           res = bp;
           break;
       }
       bp = BLOCK_NEXT(bp);
   }
   while(bp < brk){
       if (GET_ALLOC(HEAD(bp)) == 0 && GET_SIZE(HEAD(bp)) >= size){
           if(GET_SIZE(HEAD(bp)) < GET_SIZE(HEAD(res))){
               res = bp;
           }
       }
       bp = BLOCK_NEXT(bp);
   }
    return res;
    */
}

static void place(void *bp, size_t size)
{
    int left_size = GET_SIZE(HEAD(bp)) - size;
    if (left_size < (2 * DSIZE))
    {
        PUT(HEAD(bp), PACK(left_size + size, 1));
        PUT(FOOT(bp), PACK(left_size + size, 1));
    }
    else
    {
        PUT(HEAD(bp), PACK(size, 1));
        PUT(FOOT(bp), PACK(size, 1));

        char *next_bp = BLOCK_NEXT(bp);
        PUT(HEAD(next_bp), PACK(left_size, 0));
        PUT(FOOT(next_bp), PACK(left_size, 0));
    }
}

static void heap_checker()
{
    char *bp = BLOCK_NEXT(heap_listp);
    char *brk = mem_heap_hi() + 1;
    while (bp < brk)
    {
        printf("%d-%d   bp = %p, brk = %p\n", GET_SIZE(HEAD(bp)), GET_ALLOC(HEAD(bp)), bp, brk);
        bp = BLOCK_NEXT(bp);
    }
}