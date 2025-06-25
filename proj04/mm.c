/*
 * Simple, 32-bit and 64-bit clean allocator based on implicit free
 * lists, first-fit placement, and boundary tag coalescing
 * Blocks must be aligned to doubleword (8 byte).
 * boundaries. Minimum block size is 16 bytes.
 */
 
 /*
Memory layout of a block in explicit list

// Free block
[ Header (4bytes) | {Payload(empty): Next Ptr (4bytes) | Prev Ptr (4bytes) } (Nbytes) | Footer (4bytes) ]

// Allocated block
[ Header (4bytes) | Payload (Nbytes) | Footer (4bytes) ]

Header contains size of current block, and allocation status in the LSB (0 if free, 1 if alloc)
Footer has same content as header, used in coalescing with physically nearby free blocks
Next, Prev Ptrs are self-explanatory, used to structure heap as a doubly linked list
Payload is where actual stuff goes in

So for a free block, the unused payload is utilized to keep the ptr to next and prev free blocks
For allocated block, we don't care and assume the user holds the address and frees them later
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
        /* Your student ID */
        "20201572",
        /* Your full name*/
        "JiSeop Kim",
        /* Your email address */
        "smartflame@sogang.ac.kr",
};

/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ 
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  
#define ALIGNMENT 8         /* single word (4) or double word (8) alignment */
#define PLACE_THRESHOLD ALIGNMENT << 3 /* Threshold for place function: modify to find changes */

#define MAX(x, y) ((x) > (y)? (x) : (y))
#define MIN(x, y) ((x) < (y)? (x) : (y)) // Added

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) 

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)                 

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* Given block ptr bp, compute address of physical next and prev blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define NEXT_FREEP(ptr) (*(char **)(ptr))
#define PREV_FREEP(ptr) (*(char **)(ptr + WSIZE))
#define BLOCK_SIZE(ptr) (GET_SIZE(HDRP(ptr)))
#define SET(p, ptr) (*(uintptr_t *)(p) = (uintptr_t)(ptr))
/* $end mallocmacros */

/* Global variables */
static void *heap_listp = NULL; // Empty list


/* Function prototypes for internal helper routines */
static char *init_heap_space(void);

static void create_heap(char *heap_s);

static void *extend_heap(size_t words);

static char *extend_heap_if_needed(char *bp, size_t asize, int *left);

static char *fit_block(size_t asize);

static void *place(void *bp, size_t asize); // Modified return type

static void *coalesce(void *bp);

static void insert(size_t size, char *ptr);

static void delete(char *ptr);


/* packing, putting header */
#define SET_HDR(bp, size, alloc) PUT(HDRP(bp), PACK(size, alloc))

/* packing, putting footer */
#define SET_FTR(bp, size, alloc) PUT(FTRP(bp), PACK(size, alloc))

/* splitting block, handle free list */
#define SPLIT_BLK(bp, size, free) do {      \
    SET_HDR(bp, size, 1);              		\
	SET_FTR(bp, size, 1);					\
    SET_HDR(NEXT_BLKP(bp), free, 0);        \
	SET_FTR(NEXT_BLKP(bp), free, 0);        \
    insert(free, NEXT_BLKP(bp));            \
} while(0)

#define NEXT_NOT_ALLOC(ptr) (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !BLOCK_SIZE(NEXT_BLKP(ptr)))

/*
 * Initializes list pointer, heap space
 */
static char *init_heap_space(void) {
    heap_listp = NULL;
    char *heap_s = mem_sbrk(WSIZE << 2);
    if(heap_s == (void *) -1) return (void *) -1;
    return heap_s;
}

/*
 * Creates heap structure with prologue and epilogue
 */
static void create_heap(char *heap_s) {
    PUT(heap_s, 0);                             // Alignment
    PUT(WSIZE + heap_s, PACK(DSIZE, 1));        // Prologue header
    PUT((WSIZE << 1) + heap_s, PACK(DSIZE, 1)); // Prologue footer
    PUT(3 * WSIZE + heap_s, PACK(0, 1));        // Epilogue header
}

/*
 * Extends the heap with a new free block and returns pointer of new free block
 */
static void *extend_heap(size_t words) {
    void *bp;
	bp = mem_sbrk(ALIGN(words));
    if(bp == ((void *) -1)) return NULL;

    SET_HDR(bp, ALIGN(words), 0);           // Set new block header, footer (mark free)
	SET_FTR(bp, ALIGN(words), 0);
    SET_HDR(NEXT_BLKP(bp), 0, 1);               // Set epilogue header (mark end)
    insert(ALIGN(words), bp);      // insert

    return coalesce(bp);                        // coalesce and return
}

/*
 * Extends heap if needed, for a given block pointer and size
 */
static char *extend_heap_if_needed(char *bp, size_t asize, int *left) {
    if(bp == NULL) bp = extend_heap(MAX(asize, CHUNKSIZE));
    else if (left && *left < 0) {
        int extendsize = MAX(CHUNKSIZE, -(*left));
        if(extend_heap(extendsize) == NULL) return NULL;
        *left += extendsize;
    }
    return bp;
}

int mm_init(void) {
    char *heap_s = init_heap_space();
    if(heap_s != (void *) -1) {
        create_heap(heap_s);
        if(extend_heap(1 << 6) != 0) return 0;
    }
    return -1;
}

/*
 * Returns pointer to adequate block for the size by looping free list
 */
static char *fit_block(size_t asize) {
    char *bp = heap_listp;
    while (bp && BLOCK_SIZE(bp) < asize) bp = NEXT_FREEP(bp);

    return bp;
}

/*
 * Allocates block of size bytes
 */
void *mm_malloc(size_t size) {
    if (size == 0) return NULL;

    size_t asize = MAX(ALIGN(size + DSIZE), DSIZE << 1);
    char *bp = fit_block(asize);
    bp = extend_heap_if_needed(bp, asize, NULL);

    if(bp == NULL) return NULL;

    return place(bp, asize);
}


/*
 * Frees memory block pointed by ptr
 */
void mm_free(void *bp) {
    size_t size = BLOCK_SIZE(bp);
    SET_HDR(bp, size, 0);
	SET_FTR(bp, size, 0);
    insert(size, bp);
    coalesce(bp);
}

/**
 * Reallocates memory block pointed by ptr (if NULL, same as malloc)
 */
void *mm_realloc(void *ptr, size_t size) {
    if (!size) return NULL;

    size_t asize = (size <= DSIZE) ? (DSIZE << 1) + (1 << 7) : ALIGN(size + DSIZE) + (1 << 7);

    if (BLOCK_SIZE(ptr) >= asize) return ptr;

    if (NEXT_NOT_ALLOC(ptr)) {
        int left = BLOCK_SIZE(ptr) + BLOCK_SIZE(NEXT_BLKP(ptr)) - asize;
        ptr = extend_heap_if_needed(ptr, asize, &left);

        if(ptr == NULL) return NULL;  // If extend_heap_if_needed failed

        delete(NEXT_BLKP(ptr));
        SET_HDR(ptr, asize + left, 1);
		SET_FTR(ptr, asize + left, 1);
        return ptr;
    }

    size_t old_size = BLOCK_SIZE(ptr);
    size_t copy_size = MIN(old_size, size);

    // Temporarily save the data
    char *temp_data = mm_malloc(copy_size); // can change to malloc
    memcpy(temp_data, ptr, copy_size);

    // Free the old block
    mm_free(ptr);

    // Allocate new block
    void *newptr = mm_malloc(size);
    if (newptr != NULL) {
        // Copy the data to the new block
        memcpy(newptr, temp_data, copy_size);
    }

    // Free the temp buffer
    mm_free(temp_data);

    return newptr;
}


/*
 * Places block of asize bytes at start of free block bp and splits if remainder > minimum block size
 */
static void *place(void *bp, size_t asize) {
    delete(bp);

    size_t csize = BLOCK_SIZE(bp);
    size_t free_left = csize - asize;
    size_t min_block_size = DSIZE << 1;

    if (min_block_size >= free_left) {
        asize = csize; // can't be split, allocate entire block
    } else if (asize < PLACE_THRESHOLD) {
        SPLIT_BLK(bp, asize, free_left);
    } else {
        // move to next block
        SET_HDR(bp, free_left, 0);
		SET_FTR(bp, free_left, 0);
        insert(free_left, bp);
        bp = NEXT_BLKP(bp);
    }

    SET_HDR(bp, asize, 1);
	SET_FTR(bp, asize, 1);
    return bp;
}


/*
 * Finds free block, and insert (explicit list implementation)
 */
static void insert(size_t size, char *ptr) {
    char *cur_ptr = heap_listp;
    char *prev_ptr = NULL;

    /* Find place to insert */
    while (cur_ptr && BLOCK_SIZE(cur_ptr) < size) {
        prev_ptr = cur_ptr;
        cur_ptr = NEXT_FREEP(cur_ptr);
    }
    /* NOT start of list */
    if(prev_ptr != NULL) {
        SET(ptr + WSIZE, prev_ptr);
        SET(prev_ptr, ptr);
        if(cur_ptr != NULL) {
            SET(ptr, cur_ptr);
            SET(cur_ptr + WSIZE, ptr);
        } else SET(ptr, NULL);
    }
        /* start of list */
    else {
        heap_listp = ptr;
        SET(ptr + WSIZE, NULL);
        if(cur_ptr != NULL) {      // NOT the only element in list
            SET(ptr, cur_ptr);
            SET(cur_ptr + WSIZE, ptr);
        } else SET(ptr, NULL); // only element in list
    }
}


/*
 * Finds blocks by case, and deletes it
 */
static void delete(char *ptr) {
    if (PREV_FREEP(ptr) != NULL) { // Not first node
        SET(PREV_FREEP(ptr), NEXT_FREEP(ptr));
        if (NEXT_FREEP(ptr)) SET(NEXT_FREEP(ptr) + WSIZE, PREV_FREEP(ptr)); // Not last node
    } else { // first node
        heap_listp = NEXT_FREEP(ptr);
        if (heap_listp) SET(heap_listp + WSIZE, NULL); // only node
    }
}

/*
 *  Coalesces physically adjacent free blocks
 */
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // prev block allocated?
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // next block allocated?
    size_t size = BLOCK_SIZE(bp);                       // size of coalescing block

    if (prev_alloc && next_alloc) {         /* Case 1 */
        return bp;
    } else if (prev_alloc && !next_alloc) { /* Case 2 (next block is free) */
        delete(bp);
        delete(NEXT_BLKP(bp));
        size += BLOCK_SIZE(NEXT_BLKP(bp));
        SET_HDR(bp, size, 0);
        SET_FTR(bp, size, 0);
    } else if (!prev_alloc && next_alloc) { /* Case 3 (prev block is free) */
        delete(bp);
        delete(PREV_BLKP(bp));
        size += BLOCK_SIZE(PREV_BLKP(bp));
        SET_HDR(PREV_BLKP(bp), size, 0);
        SET_FTR(bp, size, 0);
        bp = PREV_BLKP(bp);
    } else {                                /* Case 4 (both blocks are free) */
        delete(bp);
        delete(PREV_BLKP(bp));
        delete(NEXT_BLKP(bp));
        size += BLOCK_SIZE(PREV_BLKP(bp)) +
                BLOCK_SIZE(NEXT_BLKP(bp));
        SET_HDR(PREV_BLKP(bp), size, 0);
        SET_FTR(NEXT_BLKP(bp), size, 0);
        bp = PREV_BLKP(bp);
    }

    insert(size, bp);

    return bp;
}
