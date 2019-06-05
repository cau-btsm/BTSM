#ifndef VM_PAGE_H
#define VM_PAGE_H

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
struct supplemental_page_table
  {
    struct hash page_map;
  };

struct supplemental_page_table_entry
  {
    void *upage;              
    void *kpage;              
                                
    struct hash_elem elem;    //해쉬 요소 설정

    enum page_status status;  // 페이지의 상태 나타냄

    bool dirty;               //더티 비트

    // for ON_SWAP
    swap_index_t swap_index;  //ON_SWAP 일때 해당하는 스왑 인덱스로 스왑 인, 스왑 아웃함

    struct file *file;
    off_t file_offset;
    uint32_t read_bytes, zero_bytes;
    bool writable;
  };


/*
 * Methods for manipulating supplemental page tables.
 */

struct supplemental_page_table* vm_supt_create (void);
void vm_supt_destroy (struct supplemental_page_table *);

bool vm_supt_install_frame (struct supplemental_page_table *supt, void *upage, void *kpage);
bool vm_supt_install_zeropage (struct supplemental_page_table *supt, void *);
bool vm_supt_set_swap (struct supplemental_page_table *supt, void *, swap_index_t);
bool vm_supt_install_filesys (struct supplemental_page_table *supt, void *page,
    struct file * file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

struct supplemental_page_table_entry* vm_supt_lookup (struct supplemental_page_table *supt, void *);
bool vm_supt_has_entry (struct supplemental_page_table *, void *page);

bool vm_supt_set_dirty (struct supplemental_page_table *supt, void *, bool);

bool vm_load_page(struct supplemental_page_table *supt, uint32_t *pagedir, void *upage);

bool vm_supt_mm_unmap(struct supplemental_page_table *supt, uint32_t *pagedir,
    void *page, struct file *f, off_t offset, size_t bytes);

void vm_pin_page(struct supplemental_page_table *supt, void *page);
void vm_unpin_page(struct supplemental_page_table *supt, void *page);

#endif