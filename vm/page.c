#include <hash.h>
#include <string.h>
#include "lib/kernel/hash.h"
#include <stdio.h>
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "filesys/file.h"

/*hash를 사용한 함수들 정의*/
static unsigned spte_hash_func(const struct hash_elem *elem, void *aux);
static bool     spte_less_func(const struct hash_elem *, const struct hash_elem *, void *aux);
static void     spte_destroy_func(struct hash_elem *elem, void *aux);


struct supplemental_page_table*
vm_supt_create (void)//가상 메모리 테이블을 만드는 함수
{
  struct supplemental_page_table *supt =
    (struct supplemental_page_table*) malloc(sizeof(struct supplemental_page_table));//malloc으로 메모리를 동적으로 할당받음

  hash_init (&supt->page_map, spte_hash_func, spte_less_func, NULL);//해시 initializing을 함
  return supt;//테이블 리턴
}

void
vm_supt_destroy (struct supplemental_page_table *supt)//가상메모리 테이블을 없애는 함수
{
  ASSERT (supt != NULL);

  hash_destroy (&supt->page_map, spte_destroy_func);//해시 없앰
  free (supt);//테이블 free
}

/**
 * upage의 시작 주소로 페이지를 설치한다.(페이지 테이블의 프레임에 존재함)
 * 성공하면 true 반환, 실패하면 false 반환
 */
bool
vm_supt_install_frame (struct supplemental_page_table *supt, void *upage, void *kpage)
{
  struct supplemental_page_table_entry *spte;
  spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));//malloc으로 메모리 할당

  spte->upage = upage;    //테이블 entry 기본 설정
  spte->kpage = kpage;
  spte->status = ON_FRAME;//프레임에 존재
  spte->dirty = false;    //더티비트 설정
  spte->swap_index = -1;  //스왑 인덱스 초기화

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&supt->page_map, &spte->elem);//해시맵에 insert
  if (prev_elem == NULL) {
    return true;//성공
  }
  else {
    // 실패
    free (spte);
    return false;
  }
}

/**
 * 새로운 가상메모리 테이블 페이지를 설치한다.
 * 모든 값이 0으로 초기화된다.
 */
bool
vm_supt_install_zeropage (struct supplemental_page_table *supt, void *upage)
{
  struct supplemental_page_table_entry *spte;
  spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));

  spte->upage = upage;//유저 페이지로 설정
  spte->kpage = NULL;
  spte->status = ALL_ZERO;//zeropage이므로 0으로 설정
  spte->dirty = false;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&supt->page_map, &spte->elem);
  if (prev_elem == NULL) return true;

  PANIC("Duplicated SUPT entry for zeropage");
  return false;
}

/**
 * 스왑 아웃할 기존 페이지를 mark한다.
 */
bool
vm_supt_set_swap (struct supplemental_page_table *supt, void *page, swap_index_t swap_index)
{
  struct supplemental_page_table_entry *spte;
  spte = vm_supt_lookup(supt, page);//lookup한다.


  if(spte == NULL) return false;

  spte->status = ON_SWAP;//스왑 상태로 변경시킨다.
  spte->kpage = NULL;
  spte->swap_index = swap_index;
  return true;
}

bool
vm_supt_install_filesys (struct supplemental_page_table *supt, void *upage,
    struct file * file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  struct supplemental_page_table_entry *spte;
  spte = (struct supplemental_page_table_entry *) malloc(sizeof(struct supplemental_page_table_entry));

  spte->upage = upage;
  spte->kpage = NULL;
  spte->status = FROM_FILESYS;
  spte->dirty = false;
  spte->file = file;
  spte->file_offset = offset;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;

  struct hash_elem *prev_elem;
  prev_elem = hash_insert (&supt->page_map, &spte->elem);
  if (prev_elem == NULL) return true;

  PANIC("Duplicated SUPT entry for filesys-page");
  return false;
}


/**
 * 테이블을 찾아 유저 페이지 어드레스가 할당된 object를 찾는다
 */
struct supplemental_page_table_entry*
vm_supt_lookup (struct supplemental_page_table *supt, void *page)
{
  // create a temporary object, just for looking up the hash table.
  struct supplemental_page_table_entry spte_temp;//temp entry를 만들어 해시 find 함수를 사용할 수 있게 한다.
  spte_temp.upage = page;
  
  struct hash_elem *elem = hash_find (&supt->page_map, &spte_temp.elem);//해시 맵에서 있는 지 찾는다.
  if(elem == NULL) {
    return NULL;
  }
  return hash_entry(elem, struct supplemental_page_table_entry, elem);//찾았으면 해시 엔트리로 변환해서 리턴한다
}

bool
vm_supt_has_entry (struct supplemental_page_table *supt, void *page)//가상메모리 테이블에 찾는 엔트리가 있으면 true를 반환하는 함수
{
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);//lookup함수를 통해 찾는다.
  if(spte == NULL) return false;//없으면 false 반환

  return true;
}

bool
vm_supt_set_dirty (struct supplemental_page_table *supt, void *page, bool value)//가상메모리 테이블의 더티비트를 설정하는 함수
{
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);
  if (spte == NULL) PANIC("set dirty - the request page doesn't exist");

  spte->dirty = spte->dirty || value;
  return true;
}

static bool vm_load_page_from_filesys(struct supplemental_page_table_entry *, void *);

bool
vm_load_page(struct supplemental_page_table *supt, uint32_t *pagedir, void *upage)//upage 주소에 있는 페이지를 로딩한다.
{
  struct supplemental_page_table_entry *spte;
  spte = vm_supt_lookup(supt, upage);//메모리 참조가 가능한 지 확인한다.
  if(spte == NULL) {
    return false;
  }

  if(spte->status == ON_FRAME) {//ON_FRAME일 경우 가상 메모리에서 메인 메모리로 이미 로딩된 것임
    return true;
  }

  void *frame_page = vm_frame_allocate(PAL_USER, upage);//페이지를 저장하기 위해 프레임 할당받음
  if(frame_page == NULL) {
    return false;
  }

  bool writable = true;
  switch (spte->status)//status에 따라 다르게 동작함
  {
  case ALL_ZERO://모두 0일경우 memset으로 페이지 사이즈만큼 프레임 페이지를 0으로 초기화
    memset (frame_page, 0, PGSIZE);
    break;

  case ON_FRAME://프레임에 이미 로딩되있을 경우 아무것도 하지않음
    break;

  case ON_SWAP://스왑 장치에 있을 경우 스왑 인으로 데이터를 불러옴
    vm_swap_in (spte->swap_index, frame_page);
    break;

  case FROM_FILESYS://파일시스템에 있을 경우
    if( vm_load_page_from_filesys(spte, frame_page) == false) {
      vm_frame_free(frame_page);
      return false;
    }

    writable = spte->writable;
    break;

  default:
    PANIC ("unreachable state");
  }
  
  // 오류가 있는 가상 메모리의 페이지 테이블 엔트리를 물리 주소로 지정함
  if(!pagedir_set_page (pagedir, upage, frame_page, writable)) {
    vm_frame_free(frame_page);//그리고 free해줌
    return false;
  }

  spte->kpage = frame_page;
  spte->status = ON_FRAME;

  pagedir_set_dirty (pagedir, frame_page, false);

  vm_frame_unpin(frame_page);

  return true;
}

bool
vm_supt_mm_unmap(
    struct supplemental_page_table *supt, uint32_t *pagedir,
    void *page, struct file *f, off_t offset, size_t bytes)//가상메모리 테이블에서 엔트리를 unload시키는 함수이다.
{
  struct supplemental_page_table_entry *spte = vm_supt_lookup(supt, page);//가상메모리 테이블에서 lookup한다
  if(spte == NULL) {
    PANIC ("munmap - some page is missing; can't happen!");
  }

  //로딩되었으면 연관된 프레임을 모두 pin 상태로 변경한다
  if (spte->status == ON_FRAME) {
    ASSERT (spte->kpage != NULL);
    vm_frame_pin (spte->kpage);//변경하지 않으면 page fault가 일어날 수 있다.
  }

  switch (spte->status)//마찬가지로 프레임 상태에 따라서 다르게 동작함
  {
  case ON_FRAME:
    ASSERT (spte->kpage != NULL);

    bool is_dirty = spte->dirty;
    is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->upage);
    is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->kpage);
    if(is_dirty) {
      file_write_at (f, spte->upage, bytes, offset);
    }

    // 페이지 맵핑을 초기화한다(free, clear를 통해)
    vm_frame_free (spte->kpage);
    pagedir_clear_page (pagedir, spte->upage);
    break;

  case ON_SWAP:
    {
      bool is_dirty = spte->dirty;
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, spte->upage);
      if (is_dirty) {
        void *tmp_page = palloc_get_page(0); //커널 영역에서 돌아가는 함수이다.
        vm_swap_in (spte->swap_index, tmp_page);
        file_write_at (f, tmp_page, PGSIZE, offset);
        palloc_free_page(tmp_page);
      }
      else {
        vm_swap_free (spte->swap_index);
      }
    }
    break;

  case FROM_FILESYS://아무것도 안함
    break;

  default:
    PANIC ("unreachable state");
  }

  hash_delete(& supt->page_map, &spte->elem);//프레임 페이지를 삭제했으므로 해시 맵도 함께 free 해준다.
  return true;
}


static bool vm_load_page_from_filesys(struct supplemental_page_table_entry *spte, void *kpage)
{
  file_seek (spte->file, spte->file_offset);

  int n_read = file_read (spte->file, kpage, spte->read_bytes);
  if(n_read != (int)spte->read_bytes)
    return false;

  ASSERT (spte->read_bytes + spte->zero_bytes == PGSIZE);
  memset (kpage + n_read, 0, spte->zero_bytes);
  return true;
}

void
vm_pin_page(struct supplemental_page_table *supt, void *page)//가상메모리에서 페이지를 pin하는 함수
{
  struct supplemental_page_table_entry *spte;
  spte = vm_supt_lookup(supt, page);
  if(spte == NULL) {
    return;
  }

  ASSERT (spte->status == ON_FRAME);
  vm_frame_pin (spte->kpage);
}

void
vm_unpin_page(struct supplemental_page_table *supt, void *page)//가상메모리에서 페이지를 unpin해주는 함수
{
  struct supplemental_page_table_entry *spte;
  spte = vm_supt_lookup(supt, page);
  if(spte == NULL) PANIC ("request page is non-existent");

  if (spte->status == ON_FRAME) {
    vm_frame_unpin (spte->kpage);
  }
}

static unsigned
spte_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct supplemental_page_table_entry *entry = hash_entry(elem, struct supplemental_page_table_entry, elem);
  return hash_int( (int)entry->upage );
}
static bool
spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct supplemental_page_table_entry *a_entry = hash_entry(a, struct supplemental_page_table_entry, elem);
  struct supplemental_page_table_entry *b_entry = hash_entry(b, struct supplemental_page_table_entry, elem);
  return a_entry->upage < b_entry->upage;
}
static void
spte_destroy_func(struct hash_elem *elem, void *aux UNUSED)
{
  struct supplemental_page_table_entry *entry = hash_entry(elem, struct supplemental_page_table_entry, elem);

  // Clean up the associated frame
  if (entry->kpage != NULL) {
    ASSERT (entry->status == ON_FRAME);
    vm_frame_remove_entry (entry->kpage);
  }
  else if(entry->status == ON_SWAP) {
    vm_swap_free (entry->swap_index);
  }

  // Clean up SPTE entry.
  free (entry);
}