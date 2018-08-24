/*
 * mm-implicit.c 
 *
 *This design is based on the simple Implicit Free List.
 *The stucture uses headers and boundry tags. The placement 
 *policy for this implementation uses the first fit. Much of the details of this 
 *implementation was derived from the book but was assembled together into one file. 
 *Next objective is to build an explicit list allocator.
 *
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
    "Lamanites",
    /* First member's full name */
    "Douglas Uriona",
    /* First member's email address */
    "duriona",
    /* Second member's full name (leave blank if none) */
    "Jonathan Wilson",
    /* Second member's email address (leave blank if none) */
    "jmw323"
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

#define WSIZE 4 //Word and header/footer size (bytes)
#define DSIZE 8  //Double word size (bytes)
#define CHUNKSIZE (1<<12) //Extend heap by this amount (bytes) = 4096 bytes

#define MAX(x,y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) //Pack a size and allocated bit into a word

/* Turn off/on HeapChecker */
//#define checkheap(verbose) mm_checkheap(verbose) //ON 
#define checkheap(verbose) //OFF

//Read and write a word at address p
#define GET(p) (*(unsigned int *)(p)) //Essentially just getting the value at this address
#define PUT(p, val) (*(unsigned int *)(p) = (val))

//Read the size and allocated fields from address p
#define GET_SIZE(p) (GET(p) & ~0x7) //looks like 11111...000 last bits are zero. Always
#define GET_ALLOC(p) (GET(p) & 0x1) //Just want the last bit

//Given block ptr p, compute address of its header and footer
#define HDRP(bp) ((char *)(bp) - WSIZE) //why are we casting this to a char???
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

//Given block ptr bp, compute address of next and previous block
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void mm_checkheap(int verbose);
static void checkblock(void *bp);

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// ********************************************************************************************************************************************************************/
int mm_init(void) //Performs the neccesary initializations such as allocating the initial heap area. Return  value should return 0 if okay, -1 if not okay
{
 if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) {
  return -1;
 }
 PUT(heap_listp, 0);  //Alignement padding
 PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); //Prologue header
 PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); //Prologue footer
 PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); // Epilogue header
 heap_listp += (2*WSIZE);

 if (extend_heap(CHUNKSIZE / WSIZE) == NULL) { //Extend the empty heap with a free block of CHUNKSIZE bytes
    return -1;
 }
 return 0;
}

 // mm_malloc - Allocate a block with at least size bytes of payload
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;

    /* $end mmmalloc */
    if (heap_listp == 0){
        mm_init();
    }
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)                                          
        asize = 2*DSIZE;                                        
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); 

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {  
        place(bp, asize);                  
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);                 
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;                                  
    place(bp, asize);                                 
	checkheap(__LINE__);
    return bp;
}

void mm_free(void *bp)
{
    if (bp == 0)
        return;

    size_t size = GET_SIZE(HDRP(bp));
    /* $end mmfree */
    if (heap_listp == 0){
        mm_init();
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
	checkheap(__LINE__);
}

 // coalesce - Boundary tag coalescing. Return ptr to coalesced block
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {                                     /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}
 // mm_realloc - Naive implementation of realloc
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

	checkheap(__LINE__);
    return newptr;
}

 // ****************The remaining routines are internal helper routines****************************************************************************************************/

 // extend_heap - Extend heap with free block and return its block pointer
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;                                        

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ 

    /* Coalesce if the previous block was free */
    return coalesce(bp);                                          
}
 // place - Place block of asize bytes at start of free block bp
 //and split if remainder would be at least minimum block size

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2*DSIZE)) {//If the size of the block minus the footer and header to placed is greater than or equal to MIN size 
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}
 // find_fit - Find a fit for a block with asize bytes

static void *find_fit(size_t asize)
{

    /* First-fit search */
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
    }
    return NULL; /* No fit */
}
/**************************************************************************************************************************************************************/


/*Print out the heapchecker info*/
static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    mm_checkheap(0);
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%u:%c] footer: [%u:%c]\n", bp, hsize, (halloc ? 'a' : 'f'), fsize, (falloc ? 'a' : 'f'));
	/*Ex output:
	*Heap (0xf618c010):
	*0xf618c010: header: [8:a] footer: [8:a]
	*0xf618c018: header: [2048:f] footer: [2048:f]
	*....
	*0xf618e018: EOL
	*/
}

static void checkblock(void *bp)
{
	//Check doubleword alignment
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);
	//Check if Header == Footer
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");
}

 // checkheap - Minimal check of the heap for consistency
void mm_checkheap(int verbose)
{
    char *bp = heap_listp;
	/*for testing */
	//printf("%p \n",verbose);
    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose)
            printblock(bp);
        checkblock(bp);
    }

    if (verbose)
        printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}
