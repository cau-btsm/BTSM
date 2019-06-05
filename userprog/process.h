#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"


tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

int process_add_file (struct file *);
struct file *process_get_file (int);
void process_close_file (int);

/*#ifdef VM
typedef int mmapid_t;

struct mmap_desc{
    mmapid_t id;
    struct list_elem elem;
    struct file* file;

    void *addr;
    size_t size;
}

#endif*/

#endif /* userprog/process.h */
