#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* Each block of allocated/unallocated memory has a header */
typedef struct {
  size_t size;
  void * prev_free;
} block_header;

/* Only blocks with unallocated memory have a footer */
typedef struct {
  size_t size;
  void *next_free;
} block_footer;

/* Each page will have a page header */
typedef struct {
  void *prev_page;
  void *next_page;
} page_header;

/* Each page has a prolog after the page header */
typedef struct {
  size_t size;
  size_t page_size;
} prolog;

/* always use 16-byte alignment */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

/* amount of space attached to each payload */
#define BLOCK_OVERHEAD (sizeof(block_header))

/* block header of a payload */
#define HDRP(bp) ((char*)(bp) - sizeof(block_header))

/* Get */
#define GET(p) (*(size_t *)(p))

/* size of a payload and its overhead */
#define GET_SIZE(p) (GET(p) & ~0xF)

/* allocation of block, 0 if allocated, 1 if unallocated */
#define GET_ALLOC(p) (GET(p) & 0x1)

/* allocation of prev. block */
#define GET_PREV_ALLOC(p) (GET(p) & 0x2)

/* put */
#define PUT(p, val) (*(size_t*)(p) = (val))

/* pack */
#define PACK(size, alloc, prev_alloc) ((size) | (alloc) | (prev_alloc))

/* block footer of a payload */
#define FTRP(bp) ((char*)(bp) + (GET_SIZE(HDRP(bp)) - sizeof(block_header) - sizeof(block_footer)))

/* the next block payload */
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))

/* the prev block payload's footer */
#define PREV_BLKP_FTR(bp) ((char*)(bp) - sizeof(block_header) - sizeof(block_footer))

/* each block footer stores a pointer to the next free block */
#define NEXT_FREE(bp) ((block_footer*)(FTRP(bp)))->next_free

/* each block header stores a pointer to the previous free block */ 
#define PREV_FREE(bp) ((block_header*) (HDRP(bp)))->prev_free

/* the prev block payload, only works if previous payload is empty */
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(PREV_BLKP_FTR(bp)))

/* rounds up to the nearest multiple of mem_pagesize() */
#define PAGE_ALIGN(size) (((size) + (mem_pagesize()-1)) & ~(mem_pagesize()-1))

/* the next page */
#define PAGE_NEXT(ph) ((page_header*)(ph))->next_page

/* the prev page */
#define PAGE_PREV(ph) ((page_header*)(ph))->prev_page

/* the first bp on a page */
#define PAGE_FIRST_BP(ph) ((char*)(ph) + PAGE_OVERHEAD - BLOCK_OVERHEAD)

/* amount of space needed for each page */
#define PAGE_OVERHEAD (sizeof(page_header) + BLOCK_OVERHEAD + BLOCK_OVERHEAD + BLOCK_OVERHEAD)

/* page prolog */
#define PAGE_PROLOG(ph) ((char*)(ph) + sizeof(page_header))

static void set_allocated(void *bp, size_t size);
static void *coalesce(void *bp);
static void checker();
static void remove_block_from_free_list(void *bp);
static void add_block_to_free_list(void *bp);
static void remove_page_of_bp(void *bp);

/* first block payload pointer */
static void *first_bp;

/* last freed block */
static void *last_freed;

/* first page pointer */
static void *first_page;

/* minimum size of a page request */
static int min_page_size;

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  min_page_size = (mem_pagesize()*7);
  size_t size = min_page_size;

  // Request a page
  first_page = mem_map(size);
  PAGE_PREV(first_page) = 0;
  PAGE_NEXT(first_page) = 0;

  // Create prolog
  PUT(HDRP(PAGE_PROLOG(first_page)), PACK(32, 0x1, 0x3));
  ((prolog*)(PAGE_PROLOG(first_page)))->page_size = size;

  // Create first bp
  first_bp = PAGE_FIRST_BP(first_page);
  PUT(HDRP(first_bp), PACK(size - PAGE_OVERHEAD, 0x0, 0x2));
  PUT(FTRP(first_bp), PACK(size - PAGE_OVERHEAD, 0x0, 0x2));

  // SET UP FREE LIST POINTERS
  NEXT_FREE(first_bp) = 0;
  PREV_FREE(first_bp) = 0;
  last_freed = first_bp;

  // Create end block
  void *bp = NEXT_BLKP(first_bp);
  PUT(HDRP(bp), PACK(0, 0x1, 0x00));

  return 0;
}

/*
 * extend - adds a new page of memory.
 *     returns the pointer to the first block payload
 */
static void *extend(size_t size) 
{
  void *lp, *pp, *bp;
  int current_avail_size = 0;

  // Calculate if we need a bigger page than the minimum
  if (size < (min_page_size - PAGE_OVERHEAD))
  {    
    current_avail_size = min_page_size;
  }
  else
  {
    current_avail_size = PAGE_ALIGN(size + PAGE_OVERHEAD);
    min_page_size = current_avail_size*25;
  }

  // find the last page
  lp = first_page;
  while (PAGE_NEXT(lp) != NULL)
  {
    lp = PAGE_NEXT(lp);
  }
  
  // Pull in a new page and link it to the other pages
  pp = mem_map(current_avail_size);
  PAGE_NEXT(lp) = pp;
  PAGE_PREV(pp) = lp;
  PAGE_NEXT(pp) = 0;

  // Create prolog
  PUT(HDRP(PAGE_PROLOG(pp)), PACK(32, 0x1, 0x3));
  ((prolog*)(PAGE_PROLOG(first_page)))->page_size = current_avail_size;

  // Set up the first bp on this page
  bp = PAGE_FIRST_BP(pp);
  PUT(HDRP(bp), PACK(current_avail_size - PAGE_OVERHEAD, 0x0, 0x2));
  PUT(FTRP(bp), PACK(current_avail_size - PAGE_OVERHEAD, 0x0, 0x2));

  // SET UP FREE LIST POINTERS
  add_block_to_free_list(bp);

  // Create end block
  bp = NEXT_BLKP(bp);
  PUT(HDRP(bp), PACK(0, 0x1, 0x00));
  
  return PAGE_FIRST_BP(pp);
}

/* 
 * mm_malloc - Allocate a block by using bytes from current_avail,
 *     grabbing a new page if necessary.
 */
void *mm_malloc(size_t size)
{
  // ALIGN THE REQUESTED SIZE TO SOMETHING MORE REASONABLE
  int new_size = ALIGN(size + BLOCK_OVERHEAD);
  void *bp;

  // IF LAST FREED == 0, THEN WE USED UP ALL THE SPACE ON ALL OF OUR PAGES
  if (last_freed != 0)
  {
    // FIND AN EMPTY BLOCK THAT IS LARGE ENOUGH
    bp = last_freed;
    do
    {
  	  if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= new_size))
      {
        set_allocated(bp, new_size);
        return bp;
      }
  	  bp = NEXT_FREE(bp);
    } while (bp != 0);
  }

  // PULL IN A NEW PAGE IF THERE ISN"T ANY CELL LARGE ENOUGH
  bp = extend(new_size);

  // ALLOCATE THE FIRST BLOCK ON THE NEW PAGE
  set_allocated(bp, new_size);
  return bp;
}

/*
 * set_allocated - sets a given empty block to allocated with size
 */
static void set_allocated(void *bp, size_t size)
{
  size_t extra_size = GET_SIZE(HDRP(bp)) - size;

  // Remove this block from the free list
  remove_block_from_free_list(bp);

  // DETERMINE IF WE NEED TO SPLIT THE BLOCK
  if (extra_size > ALIGN(1 + BLOCK_OVERHEAD))
  {
  	// SPLIT THE BLOCK
    PUT(HDRP(bp), PACK(size, 0x1, 0x2));    
    PUT(HDRP(NEXT_BLKP(bp)), PACK(extra_size, 0x0, 0x2));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(extra_size, 0x0, 0x2));

    // ADD THE NEW BLOCK TO THE FREE LIST
    add_block_to_free_list(NEXT_BLKP(bp));
  }

  // IF WE DID NOT NEED TO SPLIT THE BLOCK
  else 
  {
  	PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 0x1, 0x2));
  	PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), 0x1, 0x2));
  }
}

/*
 * mm_free - Free a block and coalesce with empty blocks around it.
 */
void mm_free(void *bp)
{
  // DEALLOCATE THE BLOCK
  PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 0x0, GET_PREV_ALLOC(HDRP(bp))));

  // BUILD A FOOTER
  PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 0x0, GET_PREV_ALLOC(HDRP(bp))));

  // COALESCE THE BLOCK WITH ADJACENT UNALLOCATED BLOCKS
  bp = coalesce(bp);

  // TELL THE NEXT BLOCK THIS BLOCK IS NOW UNALLOCATED
  PUT(HDRP(NEXT_BLKP(bp)), PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))), 0x1, 0x0));

  // IF THIS IS THE ONLY BLOCK ON THE PAGE, ERASE THIS PAGE
  if (GET_SIZE(NEXT_BLKP(bp)) == 0 && GET_PREV_ALLOC(PREV_BLKP_FTR(bp)) == 3)
  {
    remove_page_of_bp(bp);
  }
}

/*
 * mm_free - Coalesce a block with the unallocated blocks around it
 */
static void *coalesce(void *bp)
{
  size_t prev_alloc = GET_PREV_ALLOC(HDRP(bp));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));


  /* Case 1: Don't coalesce */
  if (prev_alloc && next_alloc)
  {
    // ADD THE BLOCK TO THE FREE LIST
    add_block_to_free_list(bp);
  }
  /* Case 2: Coalesce with next block */
  else if (prev_alloc && !next_alloc)
  {
     // REMOVE THE NEXT BLOCK FROM THE FREE LIST
    remove_block_from_free_list(NEXT_BLKP(bp));

    // MASH THIS BLOCK WITH THE NEXT BLOCK
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0x0, 0x2));
    PUT(FTRP(bp), PACK(size, 0x0, 0x2));

    add_block_to_free_list(bp);
  }
  /* Case 3: Coalesce with prev block */
  else if (!prev_alloc && next_alloc)
  {    
    // REMOVE THE PREVIOUS BLOCK FROM THE FREE LIST
    remove_block_from_free_list(PREV_BLKP(bp));

    // MASH THIS BLOCK WITH THE PREVIOUS BLOCK
    size += GET_SIZE(PREV_BLKP_FTR(bp));
    PUT(FTRP(bp), PACK(size, 0x0, 0x2));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0x0, 0x2));
    bp = PREV_BLKP(bp);

    add_block_to_free_list(bp);
  }
  /* Case 4: Coalesce with both prev and next blocks */
  else if (!prev_alloc && !next_alloc)
  {
    // REMOVE BOTH THE PREVIOUS BLOCK AND THE NEXT BLOCK FROM THE FREE LIST
    remove_block_from_free_list(PREV_BLKP(bp));
    remove_block_from_free_list(NEXT_BLKP(bp));

    // MASH THIS BLOCK WITH BOTH THE PREVIOUS BLOCK AND THE NEXT BLOCK
    size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(PREV_BLKP_FTR(bp));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0x0, 0x2));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0x0, 0x2));
    bp = PREV_BLKP(bp);

    add_block_to_free_list(bp);
  }
 
  return bp;
}

/*
 * mm_free - Perform a thorough check on the allocator, printing any errors that are encountered
 */
static void checker()
{
  // Check for two unallocated blocks in a row
  void *bp;
  void *pp = first_page;

  do 
  {
    bp = PAGE_FIRST_BP(pp);
    // follow the path until we get to the page_footer block
    while (GET_SIZE(HDRP(bp)) != 0) 
    {
      // if a block and it's neighbors are not allocated 
      if ((!GET_ALLOC(HDRP(bp)) && !GET_PREV_ALLOC(HDRP(bp))) || (!GET_ALLOC(HDRP(bp)) && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))))
      {
        printf("BAD_BLOCK: %p SIZE: %zu ALLOC: %zu:%zu\n", PREV_BLKP(bp), GET_SIZE(HDRP(PREV_BLKP(bp))), GET_PREV_ALLOC(HDRP(PREV_BLKP(bp))), GET_ALLOC(HDRP(PREV_BLKP(bp))));
        printf("BAD_BLOCK: %p SIZE: %zu ALLOC: %zu:%zu\n", bp, GET_SIZE(HDRP(bp)), GET_PREV_ALLOC(HDRP(bp)), GET_ALLOC(HDRP(bp)));
        printf("BAD_BLOCK: %p SIZE: %zu ALLOC: %zu:%zu\n", NEXT_BLKP(bp), GET_SIZE(HDRP(NEXT_BLKP(bp))), GET_PREV_ALLOC(HDRP(NEXT_BLKP(bp))), GET_ALLOC(HDRP(NEXT_BLKP(bp))));
        printf("Two unallocated blocks in a row!\n");
      }

      // if a block is not allocated and is not in the free list
      if (!GET_ALLOC(HDRP(bp)))
      {
        int b = 1;
        void *p = last_freed;
        while (p != 0)
        {
          if (p == bp)
          {
            b = 0;
          }
          p = NEXT_FREE(p);
        }
        if (b == 1)
        {
          printf("An unallocated block is not in the free list!\n");
        }
      }
      bp = NEXT_BLKP(bp);
    }
  } while ((pp = PAGE_NEXT(pp)) != NULL);

  // Check that ever block in free list is marked as free
  void *p = last_freed;
  while (p != 0){
    if (GET_ALLOC(HDRP(p))){      
      printf("A block in the free list is allocated!\n");
      printf("BAD_BLOCK: %p SIZE: %zu ALLOC: %zu:%zu\n", bp, GET_SIZE(HDRP(bp)), GET_PREV_ALLOC(HDRP(bp)), GET_ALLOC(HDRP(bp)));
    }
    p = NEXT_FREE(p);
  }

}

/*
 * remove_block_from_free_list(bp)- Call on an block payload pointer to remove the block from
 *                                   the list of unallocated blocks.
 */
static void remove_block_from_free_list(void *bp)
{
  if ((PREV_FREE(bp) == 0) && (NEXT_FREE(bp) == 0))
  {    
  	last_freed = 0;
  }
  else if (PREV_FREE(bp) == 0)
  {
  	PREV_FREE(NEXT_FREE(bp)) = 0;
    last_freed = NEXT_FREE(bp);
  }
  else if (NEXT_FREE(bp) == 0)
  {
   	NEXT_FREE(PREV_FREE(bp)) = 0;
  }
  else 
  {
    NEXT_FREE(PREV_FREE(bp)) = NEXT_FREE(bp);
  	PREV_FREE(NEXT_FREE(bp)) = PREV_FREE(bp);
  }	
}

/*
 * add_block_to_free_list(bp) - Add a given block to the list of unallocated blocks
 */
static void add_block_to_free_list(void *bp)
{
  if (last_freed == 0) 
  {
    NEXT_FREE(bp) = 0;
    PREV_FREE(bp) = 0;
    last_freed = bp;
  }
  else 
  {
	  NEXT_FREE(bp) = last_freed;
    PREV_FREE(bp) = 0;
    PREV_FREE(last_freed) = bp;
    last_freed = bp;
  }
}

/*
 * remove_page_of_bp - Unmap pages used by allocator at given block pointer
 */
static void remove_page_of_bp(void *bp)
{
  void *pp = ((char *)(bp) - PAGE_OVERHEAD + BLOCK_OVERHEAD);
  if ((PAGE_PREV(pp) == 0) && (PAGE_NEXT(pp) == 0))
  {
    return;
  }
  else if (PAGE_PREV(pp) == 0)
  {
    PAGE_PREV(PAGE_NEXT(pp)) = 0;
    first_page = PAGE_NEXT(pp);
  }
  else if (PAGE_NEXT(pp) == 0)
  {
    PAGE_NEXT(PAGE_PREV(pp)) = 0;
  }
  else 
  {
    PAGE_NEXT(PAGE_PREV(pp)) = PAGE_NEXT(pp);
    PAGE_PREV(PAGE_NEXT(pp)) = PAGE_PREV(pp);
  }
  mem_unmap(pp, ((prolog*)(PAGE_PROLOG(first_page)))->page_size);
}