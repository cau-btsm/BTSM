#ifndef VIRTUALMEMORY_FRAME_H
#define VIRTUALMEMORY_FRAME_H

#include <hash.h>
#include "lib/kernel/hash.h"

#include "threads/synch.h"
#include "threads/palloc.h"
void virtualmemory_frame_free (void*);
void virtualmemory_frame_remove_entry (void*);
void virtualmemory_frame_unpin(void *kpage);
void virtualmemory_frame_pin(void *kpage);
void virtualmemory_frame_init (void);
void* virtualmemory_frame_allocate (enum palloc_flags flags, void *upage);

#endif /* vm/frame.h */
