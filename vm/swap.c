#include <bitmap.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "vm/swap.h"
#include <stdio.h>
static struct block *swap_block;
static struct bitmap *swap_available;

static const size_t SECTORS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;//섹터 당 페이지 갯수 설정

static size_t swap_size;
static int swapin=0;//for debugging
static int swapout=0;
void
virtualmemory_swap_init ()
{
  ASSERT (SECTORS_PER_PAGE > 0);

  swap_block = block_get_role(BLOCK_SWAP);//block get role 함수를 통해 스왑 블럭을 초기화한다
  if(swap_block == NULL) {
    PANIC ("스왑 블럭을 초기화할 수 없다.");
    NOT_REACHED ();
  }

  swap_size = block_size(swap_block) / SECTORS_PER_PAGE;//스왑 사이즈 설정
  swap_available = bitmap_create(swap_size);

  bitmap_set_all(swap_available, true);
}


swap_index_t virtualmemory_swap_out (void *page)
{
   // printf("swap_out: %d\n",++swapout);//for debugging

  ASSERT (page >= PHYS_BASE); // 페이지가 사용자의 가상 메모리에 있는지 확인

 
  size_t swap_index = bitmap_scan (swap_available, /*start*/0, /*cnt*/1, true);

  size_t i;
  for (i = 0; i < SECTORS_PER_PAGE; ++ i) {
    block_write(swap_block,
        /* 섹터 넘버 */  swap_index * SECTORS_PER_PAGE + i,
        /* 타겟 주소 */ page + (BLOCK_SECTOR_SIZE * i)
        );
  }
  bitmap_set(swap_available, swap_index, false);
  return swap_index;
}


void virtualmemory_swap_in (swap_index_t swap_index, void *page)//가상 메모리에 있는 블럭을 swap in 하기 위한 함수
{
 // printf("swap_in: %d\n",++swapin); //for debugging

  ASSERT (page >= PHYS_BASE);

  ASSERT (swap_index < swap_size);//스왑 영역 확인
  if (bitmap_test(swap_available, swap_index) == true) {
    PANIC ("허용되지 않는 접근입니다");
  }

  size_t i;
  for (i = 0; i < SECTORS_PER_PAGE; ++ i) {
    block_read (swap_block,swap_index * SECTORS_PER_PAGE + i, page + (BLOCK_SECTOR_SIZE * i));//블럭 단위로 읽는다
  }

  bitmap_set(swap_available, swap_index, true);
}

void
virtualmemory_swap_free (swap_index_t swap_index)//스왑 공간이 충분한 지 확인하는 함수
{
  ASSERT (swap_index < swap_size);
  if (bitmap_test(swap_available, swap_index) == true) {
    PANIC ("할당되지않은 스왑 블럭에 접근하려고 했습니다");
  }
  bitmap_set(swap_available, swap_index, true);
}