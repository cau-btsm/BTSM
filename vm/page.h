#ifndef VIRTUALMEMORY_PAGE_H
#define VIRTUALMEMORY_PAGE_H

#include "vm/swap.h"
#include <hash.h>
#include "filesys/off_t.h"

/**
 * Indicates a state of page.
 */
enum page_status {
  ALL_ZERO,         // All zeros
  ON_FRAME,         // Actively in memory
  ON_SWAP,          // Swapped (on swap slot)
  FROM_FILESYS      // from filesystem (or executable)
};

/**
 * Supplemental page table. The scope is per-process.
 */
struct page_table
  {
    /* The hash table, page -> spte */
    struct hash page_map;
  };

struct page_table_entry
  {
    void *upage;              /* Virtual address of the page (the key) */
    void *kpage;              /* Kernel page (frame) associated to it.
                                 Only effective when status == ON_FRAME.
                                 If the page is not on the frame, should be NULL. */
    struct hash_elem hash_element;

    enum page_status status;

    bool dirty;               /* Dirty bit. */

    // for ON_SWAP
    swap_index_t swap_index;  /* Stores the swap index if the page is swapped out.
                                 Only effective when status == ON_SWAP */

    // for FROM_FILESYS
    struct file *file;
    off_t file_offset;
    uint32_t read_bytes, zero_bytes;
    bool writable;
  };


/*
 * Methods for manipulating supplemental page tables.
 */

struct page_table* virtualmemory_table_create (void);
void virtualmemory_table_destroy (struct page_table *);

bool virtualmemory_table_install_frame (struct page_table *table, void *upage, void *kpage);
bool virtualmemory_table_install_zeropage (struct page_table *table, void *);
bool virtualmemory_table_set_swap (struct page_table *table, void *, swap_index_t);
bool virtualmemory_table_install_filesys (struct page_table *table, void *page,
    struct file * file, off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

struct page_table_entry* virtualmemory_table_lookup (struct page_table *table, void *);
bool virtualmemory_table_has_entry (struct page_table *, void *page);

bool virtualmemory_table_set_dirty (struct page_table *table, void *, bool);

bool virtualmemory_load_page(struct page_table *table, uint32_t *pagedir, void *upage);

bool virtualmemory_table_mm_unmap(struct page_table *table, uint32_t *pagedir,
    void *page, struct file *f, off_t offset, size_t bytes);

void virtualmemory_pin_page(struct page_table *table, void *page);
void virtualmemory_unpin_page(struct page_table *table, void *page);

#endif