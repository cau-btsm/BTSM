#ifndef VIRTUALMEMORY_PAGE_H
#define VIRTUALMEMORY_PAGE_H

#include "vm/swap.h"
#include <hash.h>
#include "filesys/off_t.h"

/**
 * 페이지 상태를 나타내는 enum
 */
enum page_status {
  ALL_ZERO,         // 0
  ON_FRAME,         // 메인메모리 안에있다
  ON_SWAP,          // 스왑 장치 안에있다
  FROM_FILESYS      // 파일시스템 안에 있다
};

/**
 * 프로세스당 하나 만들어지는 페이지 테이블 정의
 */
struct page_table
  {
    struct hash page_map;
  };

struct page_table_entry
  {
    struct file *file;
    off_t file_offset;
    uint32_t read_bytes, zero_bytes;
    bool writable;
    void *upage;    
    void *kpage;        
    struct hash_elem hash_element;//해쉬 요소 설정
    enum page_status status;// 페이지의 상태 나타냄
    bool dirty;               //더티 비트
    swap_index_t swap_index;  //ON_SWAP 일때 해당하는 스왑 인덱스로 스왑 인, 스왑 아웃함
  };

/*페이지 테이블을 위한 함수 원형 선언 부분*/

struct page_table* virtualmemory_table_create (void);
void virtualmemory_table_destroy (struct page_table *);
bool virtualmemory_table_install_frame (struct page_table *table, void *upage, void *kpage);
bool virtualmemory_table_install_zeropage (struct page_table *table, void *);
bool virtualmemory_table_set_swap (struct page_table *table, void *, swap_index_t);
bool virtualmemory_table_install_filesys (struct page_table *table, void *page, struct file * file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
struct page_table_entry* virtualmemory_table_lookup (struct page_table *table, void *);
bool virtualmemory_table_has_entry (struct page_table *, void *page);
bool virtualmemory_table_set_dirty (struct page_table *table, void *, bool);
bool virtualmemory_load_page(struct page_table *table, uint32_t *pagedir, void *upage);
bool virtualmemory_table_mm_unmap(struct page_table *table, uint32_t *pagedir,void *page, struct file *f, off_t offset, size_t bytes);
void virtualmemory_pin_page(struct page_table *table, void *page);
void virtualmemory_unpin_page(struct page_table *table, void *page);

#endif