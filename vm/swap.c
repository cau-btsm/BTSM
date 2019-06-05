#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "vm/swap.h"
#include <stdio.h>
static struct block *swap_block;
static struct bitmap *swap_available;

static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

// the number of possible (swapped) pages.
static size_t swap_size;
static int swapin=0;
static int swapout=0;
void
virtualmemory_swap_init ()
{
 // printf("SWAP INIT@@@@@@@@@@@@@@@@\n");
  ASSERT (SECTORS_PER_PAGE > 0); // 4096/512 = 8?

  // Initialize the swap disk
  swap_block = block_get_role(BLOCK_SWAP);
  if(swap_block == NULL) {
    PANIC ("Error: Can't initialize swap block");
    NOT_REACHED ();
  }

  // Initialize swap_available, with all entry true
  // each single bit of `swap_available` corresponds to a block region,
  // which consists of contiguous [SECTORS_PER_PAGE] sectors,
  // their total size being equal to PGSIZE.
  swap_size = block_size(swap_block) / SECTORS_PER_PAGE;
  swap_available = bitmap_create(swap_size);
//  printf("SWAP AV : %d\n",swap_size);

  bitmap_set_all(swap_available, true);
}


swap_index_t virtualmemory_swap_out (void *page)
{
    printf("swap_out: %d\n",++swapout);
 // printf("SWAP UT\n");
  // Ensure that the page is on user's virtual memory.
  ASSERT (page >= PHYS_BASE);

  // Find an available block region to use
  size_t swap_index = bitmap_scan (swap_available, /*start*/0, /*cnt*/1, true);

  size_t i;
  for (i = 0; i < SECTORS_PER_PAGE; ++ i) {
  //  printf("ATTEMPT TO BLOCK WRITE\n");
    block_write(swap_block,
        /* sector number */  swap_index * SECTORS_PER_PAGE + i,
        /* target address */ page + (BLOCK_SECTOR_SIZE * i)
        );
  }

  // occupy the slot: available becomes false
  bitmap_set(swap_available, swap_index, false);
 // printf("RETURN SWAP INDEX\n");
  return swap_index;
}


void virtualmemory_swap_in (swap_index_t swap_index, void *page)
{
  printf("swap_in: %d\n",++swapin);
//  printf("SWAP IN\n");
  // Ensure that the page is on user's virtual memory.
  ASSERT (page >= PHYS_BASE);

  // check the swap region
  ASSERT (swap_index < swap_size);
  if (bitmap_test(swap_available, swap_index) == true) {
    // still available slot, error
    PANIC ("Error, invalid read access to unassigned swap block");
  }

  size_t i;
  for (i = 0; i < SECTORS_PER_PAGE; ++ i) {
    block_read (swap_block,
        /* sector number */  swap_index * SECTORS_PER_PAGE + i,
        /* target address */ page + (BLOCK_SECTOR_SIZE * i)
        );
  }

  bitmap_set(swap_available, swap_index, true);
}

void
virtualmemory_swap_free (swap_index_t swap_index)
{
  // check the swap region
  //printf("SWAP FREE\n");
  ASSERT (swap_index < swap_size);
  if (bitmap_test(swap_available, swap_index) == true) {
    PANIC ("Error, invalid free request to unassigned swap block");
  }
  bitmap_set(swap_available, swap_index, true);
}