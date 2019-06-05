#include <hash.h>
#include <list.h>
#include <stdio.h>
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"

#include "vm/page.h"
#include "vm/swap.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

enum SwapPolicy {//SWAP 정책이 3가지가 있기 때문에 enum으로 정의하였다.
  CLOCK,
  SECOND_CHANCE,
  LRU
};
static int historyNum = 0;/* LRU를 위한 static 변수이다. */

static enum SwapPolicy Policy = SECOND_CHANCE;//Swap Policy

/* 크리티컬 섹션을 위한 뮤텍스 선언 */
static struct lock frame_lock;

/* 물리 주소를 프레임 테이블 엔트리에 맵핑하기 위한 hash map */
static struct hash frame_map;

/* Swap 정책 중 clock 알고리즘을 위한 list 선언 */
static struct list frame_list;      
static struct list_elem *clock_ptr; //clock pointer

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux);
static bool     frame_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);

/**
 * 프레임 테이블 엔트리
 */
struct frame_table_entry
  {
    void *upage;               /* User (Virtual Memory) Address, pointer to page */
    void *kpage;               /* 물리 주소에 맵핑되는 커널 페이지 */

    struct hash_elem hashmap;    /* 해시 맵 */
    struct list_elem listmap;    /* 프레임 리스트 */

    struct thread *t;          /* 연관된 스레드 */

    bool pinned; /*중요한 프로세스의 eviction을 방지하기 위한 pinned 변수*/

    int history;/*LRU 정책을 위한 변수*/
  };


static struct frame_table_entry* pick_frame_to_evict(uint32_t* pagedir);
static void virtualmemory_frame_do_free (void *kpage, bool free_page);


void
virtualmemory_frame_init ()//가상 메모리 initializing
{
  lock_init (&frame_lock);
  hash_init (&frame_map, frame_hash_func, frame_less_func, NULL);//hash map initializing
  list_init (&frame_list);
  clock_ptr = NULL;//clock 포인터 초기화
}

/**
 * 새로운 프레임을 할당하고 연관된 페이지의 주소를 반환하는 함수
 */
void*
virtualmemory_frame_allocate (enum palloc_flags flags, void *upage)
{
  lock_acquire (&frame_lock);//mutual exclusion을 위함

  void *frame_page = palloc_get_page (PAL_USER | flags);
  if (frame_page == NULL) {//if frame table is full, 스왑이 필요하다

    /* 스왑해 낼 페이지를 고르고 스왑아웃한다 */
    struct frame_table_entry *f_picked = pick_frame_to_evict( thread_current()->pagedir );//스왑할 프레임을 pick한다.
  //  printf("f_picked: %x th=%x, pagedir = %x, up = %x, kp = %x, hash_size=%d\n", f_picked, f_picked->t, /*임시로 picked 된 프레임 출력 */
   //     f_picked->t->pagedir, f_picked->upage, f_picked->kpage, hash_size(&frame_map));

    ASSERT (f_picked != NULL && f_picked->t != NULL);

    // 페이지 맵핑을 지우고 스왑으로 대체한다.
    ASSERT (f_picked->t->pagedir != (void*)0xcccccccc);
    pagedir_clear_page(f_picked->t->pagedir, f_picked->upage);

    bool is_dirty = false;//더티 비트 설정
    is_dirty = is_dirty || pagedir_is_dirty(f_picked->t->pagedir, f_picked->upage);
    is_dirty = is_dirty || pagedir_is_dirty(f_picked->t->pagedir, f_picked->kpage);

    swap_index_t swap_idx = virtualmemory_swap_out( f_picked->kpage );//스왑 인덱스 설정한다.

    virtualmemory_table_set_swap(f_picked->t->table, f_picked->upage, swap_idx);//스왑을 설정한다.

    virtualmemory_table_set_dirty(f_picked->t->table, f_picked->upage, is_dirty);//더티 비트를 설정한다.

    virtualmemory_frame_do_free(f_picked->kpage, true); // 스왑한 페이지를 free시켜준다.

    frame_page = palloc_get_page (PAL_USER | flags);//프레임 테이블에 다시 할당시켜준다.
    ASSERT (frame_page != NULL);
  }

  struct frame_table_entry *frame = malloc(sizeof(struct frame_table_entry));//프레임 테이블에 넣기 위해 프레임을 만든다.
  if(frame == NULL) {
    // 프레임 할당에 실패하면 NULL을 리턴한다.
    lock_release (&frame_lock);
    return NULL;
  }

  frame->t = thread_current ();
  frame->upage = upage;
  frame->kpage = frame_page;
  frame->pinned = true;

  frame->history = historyNum++;//LRU를 위한 변수 증가
  // 해시 테이블에 넣는다.
  hash_insert (&frame_map, &frame->hashmap);
  list_push_back (&frame_list, &frame->listmap);//list map에도 제일 뒤에 삽입한다.
  lock_release (&frame_lock);
  return frame_page;
}

/**
 * Deallocate a frame or page.
 */
void
virtualmemory_frame_free (void *kpage)//가상 메모리 프레임을 free한다.
{
  lock_acquire (&frame_lock);
  virtualmemory_frame_do_free (kpage, true);
  lock_release (&frame_lock);
}

/**
 *페이지 해제는 하지 않고 단순히 프레임 테이블에서만 제거하는 함수.
 */
void
virtualmemory_frame_remove_entry (void *kpage)//remove entry from frame
{
  lock_acquire (&frame_lock);
  virtualmemory_frame_do_free (kpage, false);
  lock_release (&frame_lock);
}

/**
 * 프레임이나 페이지를 할당 해제한다
 */
void
virtualmemory_frame_do_free (void *kpage, bool free_page)
{
  ASSERT (lock_held_by_current_thread(&frame_lock) == true);
  ASSERT (is_kernel_vaddr(kpage));
  ASSERT (pg_ofs (kpage) == 0);

  struct frame_table_entry f_tmp;
  f_tmp.kpage = kpage;//임시 변수를 사용하여 해시 맵에서 요소를 찾는다.

  struct hash_elem *h = hash_find (&frame_map, &(f_tmp.hashmap));
  if (h == NULL) {
    PANIC ("free할 페이지가 해시 맵에 존재하지 않는다.");
  }

  struct frame_table_entry *f;
  f = hash_entry(h, struct frame_table_entry, hashmap);//hash entry를 리턴받는다.

  hash_delete (&frame_map, &f->hashmap);//삭제한다.
  list_remove (&f->listmap);

  // Free resources
  if(free_page) palloc_free_page(kpage);//페이지를 free시킨다.
  free(f);
}


struct frame_table_entry* clock_frame_next(void);/*clock 알고리즘을 위한 함수*/

struct frame_table_entry* lru_entry(void);/* LRU 알고리즘을 위한 함수*/

struct frame_table_entry* pick_frame_to_evict( uint32_t *pagedir ){//스왑할 페이지를 고르기 위한 함수
  if(Policy == CLOCK){// If swap policy is CLOCK,

     size_t n = hash_size(&frame_map);
    if(n == 0) PANIC("프레임 테이블이 비어있습니다!");

    size_t it;
    for(it = 0; it <= n + n; ++ it) //루프를 통해 clock 포인터를 이동시킨다.
    {
      struct frame_table_entry *e = clock_frame_next();

      if(e->pinned) {//pinned일 경우 스왑하면 안되므로 continue한다
        continue;
      }
      if( pagedir_is_accessed(pagedir, e->upage)) {//해당 함수를 이용해 true를 false로 만들어준다.(비트를 1에서 0으로 설정)
        pagedir_set_accessed(pagedir, e->upage, false);
        continue;
      }
      // 아래 반환되는 e는 스왑될 프레임이다.
      return e;
    }

  } else if(Policy == SECOND_CHANCE){//second chance일 경우,
    
     size_t n = hash_size(&frame_map);
    if(n == 0) PANIC("프레임 테이블이 비어있습니다!");

    size_t it;
    for(it = 0; it <= n; ++ it) //iterator를 사용해 반복,
    {
      clock_ptr=NULL;
      struct frame_table_entry *e = clock_frame_next();//clock 알고리즘과 마찬가지로 포인터를 이동시키지만,
                                                      //end와 start가 연결되어 있지 않음!! (Queue 형식으로 구현)

      if(e->pinned) {
        continue;
      }

      if( pagedir_is_accessed(pagedir, e->upage)) {// second chance를 지급한다.
         printf("CALL SET\n");
        pagedir_set_accessed(pagedir, e->upage, false);
        continue;
      }

      return e;//교체될 프레임 리턴
    }

  } else if(Policy == LRU){//LRU 정책일경우
    size_t n = hash_size(&frame_map);
    if(n == 0) PANIC("프레임 테이블이 비어있습니다!");

      struct frame_table_entry *e = lru_entry();//LRU_ENTRY 반환

      return e;//교체될 프레임 리턴

  }

  PANIC ("어느 프레임도 스왑아웃 할 수 없습니다.\n");
};

struct frame_table_entry* lru_entry(void){//LRU 알고리즘 구현
    struct frame_table_entry *e;

    if(clock_ptr==NULL){
      clock_ptr=list_begin(&frame_list);//리스트의 첫번째 원소를 가리키게 함
    }
    int oldest = 999999999;//가장 오래된 것을 큰 값으로 설정 (숫자가 낮을 수록 오래된 것)

    for(clock_ptr=list_begin(&frame_list); clock_ptr!=list_end(&frame_list); clock_ptr=list_next(clock_ptr)){
      if(list_entry(clock_ptr, struct frame_table_entry, listmap)->history < oldest && (list_entry(clock_ptr, struct frame_table_entry, listmap)->pinned)==false){
        oldest= list_entry(clock_ptr, struct frame_table_entry, listmap)->history;//오래된 것을 찾으면 그 값과 포인터 저장(스왑될 수 있게)
        e=list_entry(clock_ptr, struct frame_table_entry, listmap);//포인터 저장
      }
    }
    e->history=99999999;//다음에 다시 picked 될 수 없게 큰 값으로 설정한다.

   
    return e;
}
struct frame_table_entry* clock_frame_next(void)
{
  if (list_empty(&frame_list)) PANIC("프레임 테이블이 비어있습니다!");

  if (clock_ptr == NULL || clock_ptr == list_end(&frame_list))//clock 포인터가 null이나 끝을 가리키고 있으면
    clock_ptr = list_begin (&frame_list);//시작을 가리키게 한다
  else
    clock_ptr = list_next (clock_ptr);//그게 아니라면 리스트의 다음 요소를 가리키게 한다.
  
  struct frame_table_entry *e = list_entry(clock_ptr, struct frame_table_entry, listmap);//그 가리키고 있는 요소를 반환함
  return e;
}

static void virtualmemory_frame_set_pinned(void *kpage,bool new_value){//pin을 true 또는 false로 설정하는 함수
  lock_acquire(&frame_lock);//mutual exclusion을 위한 뮤텍스

  struct frame_table_entry f_tmp;
  f_tmp.kpage = kpage;
  struct hash_elem *h = hash_find(&frame_map, &(f_tmp.hashmap));//커널 페이지를 해시맵에서 찾는다.

  if(h==NULL){
    PANIC("pin/unpin할 프레임이 존재하지 않습니다");
  }

  struct frame_table_entry *f;//해시맵에 존재하므로
  f = hash_entry(h,struct frame_table_entry, hashmap);//엔트리를 반환받아서
  f->pinned = new_value;//새로운 pin value를 설정한다.

  lock_release(&frame_lock);

}

void virtualmemory_frame_unpin(void *kpage){//프레임의 pin 해제
  virtualmemory_frame_set_pinned(kpage,false);
}

void virtualmemory_frame_pin(void *kpage){//프레임의 pin 설정
  virtualmemory_frame_set_pinned(kpage, true);
}


static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)//해싱을 위한 함수
{
  struct frame_table_entry *entry = hash_entry(elem, struct frame_table_entry, hashmap);
  return hash_bytes( &entry->kpage, sizeof entry->kpage );
}
static bool frame_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)//해싱을 위한 함수
{
  struct frame_table_entry *a_entry = hash_entry(a, struct frame_table_entry, hashmap);
  struct frame_table_entry *b_entry = hash_entry(b, struct frame_table_entry, hashmap);
  return a_entry->kpage < b_entry->kpage;
}
