#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h" // 우리가 만든 frame.h헤더를 include하였다.
#include "vm/page.h"

#ifndef VM

#define virtualmemory_frame_allocate(x, y) palloc_get_page(x) //매크로 함수를 사용하여 가상메모리 프레임테이블을 할당하면
#define virtualmemory_frame_free(x) palloc_free_page(x)       //자동으로 페이지도 함께 할당되게 구현하였다.

#endif

/* Parameters for user program execution
   len: the length of a program command line 
   cmdline: a string pointer to program command line */
struct uprg_params {
  int len;
  char cmdline[512];
};

static thread_func start_process NO_RETURN;
static bool load (struct uprg_params *params, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *cmdline) 
{
  char *c;
  tid_t tid;
  struct uprg_params *params;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  params = palloc_get_page(PAL_ZERO);
  if (params == NULL) {
    return TID_ERROR;
  }
  strlcpy (params->cmdline, cmdline, PGSIZE - sizeof (struct uprg_params));
  params->len = strlen(params->cmdline);

  /* Prepare argv */
  c = params->cmdline;
  while (*c) {
    if (*c == ' ') {
      *c = 0;
    }
    c++;
  }

  /* Create a new thread to execute cmdline. */
  tid = thread_create (params->cmdline, PRI_DEFAULT, start_process, params);
  if (tid == TID_ERROR)
    palloc_free_page (params); 

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *_params)
{
  struct thread *t;
  struct uprg_params *params = _params;

  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  
  
  success = load (params, &if_.eip, &if_.esp);

  /* It is okay to continue exec in parent process. */
  t = thread_current ();
  sema_up (&t->load_sema);

  /* If load failed, quit. */
  palloc_free_page (params);
  if (!success) {
    thread_exit ();
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
  struct thread *child;
  int exit_status;

  if (!(child = thread_get_child(child_tid)))
    return -1;

  sema_down (&child->wait_sema);
  list_remove (&child->child_elem);
  exit_status = child->exit_status;
  sema_up (&child->destroy_sema);
  
  return exit_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;


  #ifdef VM
  virtualmemory_table_destroy(cur->table);//SONGMINJOON
  cur->table=NULL;
  #endif
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

int
process_add_file (struct file *f)
{
  struct thread *t;
  int fd;
  if (f == NULL)
    return -1;
  t = thread_current ();
  fd = t->next_fd++;
  t->fd_table[fd] = f;
  return fd;
}

struct file *
process_get_file (int fd)
{
  struct thread *t = thread_current ();
  if (fd <= 1 || t->next_fd <= fd)
    return NULL;
  return t->fd_table[fd];
}

void process_close_file (int fd)
{
  struct thread *t = thread_current ();
  if (fd <= 1 || t->next_fd <= fd)
    return;
  file_close (t->fd_table[fd]);
  t->fd_table[fd] = NULL;
}





/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (struct uprg_params *params, void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (struct uprg_params *params, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  char *file_name;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();

  #ifdef VM
  t->table=virtualmemory_table_create();//SONGMINJOON
  #endif

  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file_name = params->cmdline;
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (params, esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      
      // MINJUN

      uint8_t *kpage = virtualmemory_frame_allocate (PAL_USER,upage);//page_allocation 대신, 매크로 함수로 만든 가상메모리 프레임 할당 함수를 사용하였다.


      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          // MINJUN

          virtualmemory_frame_free (kpage);//page free 함수 대신 우리가 만든 가상메모리 free 함수를 사용하였다.


          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          // MINJUN

          virtualmemory_frame_free (kpage);//page free 함수 대신 우리가 만든 가상메모리 free 함수를 사용하였다.


          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;

#ifdef VM
      ofs += PGSIZE;
#endif

    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (struct uprg_params *params, void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  int i, t, argc;
  uint32_t addr; 
  
  // MINJUN

  kpage = virtualmemory_frame_allocate(PAL_USER | PAL_ZERO, PHYS_BASE - PGSIZE);////page_allocation 대신, 매크로 함수로 만든 가상메모리 프레임 할당 함수를 사용하였다.


  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success) {
        *esp = PHYS_BASE;

        /* Push argvs to the new task's stack 
          (the stack grows downward)
           | esp | argv data | word align | array of pointers to argv data |
           | -- function arguments | fake ret addr | */

        /* Make stack space for allocation of argv data */ 
        addr = (uint32_t) *esp;
        *esp = (void *) (addr - (params->len + 1));
#ifdef CSLDEBUG
        printf("num of params: %d\n", params->len);
#endif

        /* Push argv data (bytewise) */
        for (i=0; i<params->len+1; i++) {
          addr = (uint32_t) *esp;
          ((char *) addr)[i] = params->cmdline[i]; 
#ifdef CSLDEBUG
          printf("%c\n", ((char *) addr)[i]);
#endif
        }
#ifdef CSLDEBUG
        printf("0x%8X: \n", (uint32_t) *esp);
#endif

        /* Word align */
        addr = (uint32_t) *esp;
        t = addr - (params->len + 1) % 4;
        *esp = (void *) t; 

        /* Zerofill the align */
        for (i=0; i<(params->len + 1) % 4; i++) {
          addr = (uint32_t) *esp;
          ((char *) addr)[i] = 0;
#ifdef CSLDEBUG
          printf("%d\n", ((char *) *esp)[i]);
#endif
        }
#ifdef CSLDEBUG
        printf("0x%8X: \n", (uint32_t) *esp);
#endif

        /* Allocate the last argv elem (argv[last] = NULL),
           Assume that the address points to the NULL pointer variable */
        addr = ((uint32_t) *esp) - (sizeof (char *));
        *esp = (void *) addr;
        *((char **) addr) = 0;
#ifdef CSLDEBUG
        printf("0x%8X: argv[last]\n", *((uint32_t *) *esp));
        printf("0x%8X: argv[last]\n", (uint32_t) *esp);
#endif

        /* Get argc, then allocate and init argv pointer array */
        t = 0;
        argc = 0;
        for (i=0; i<=params->len; i++) {
          addr = (uint32_t) PHYS_BASE - 1 - i;
#ifdef CSLDEBUG
          printf("i[%d] it points to: %c\n", i, *(char *) addr);
#endif
          if (t && !*(char *) addr) {
            /* When found a token, count up argc */
            argc++;

            /* Designate each start addr of argv string */
            *esp = (void *) ((uint32_t) *esp) - (sizeof (char *));

            /* addr is 0, actual data is just before, */
            *((uint32_t *) *esp) = addr + 1;
#ifdef CSLDEBUG
            printf("0x%8X: points to %s\n", (uint32_t) *esp, *((char **) *esp));
#endif 
          }

          if (i == params->len) {
            /* Treat like we found a token, count up argc */
            argc++;

            /* Designate each start addr of argv string */
            *esp = (void *) ((uint32_t) *esp) - (sizeof (char *));
            
            /* This designates to argv[0] */
            *((uint32_t *) *esp) = addr;
#ifdef CSLDEBUG
            printf("0x%8X: points to %s\n", (uint32_t) *esp, *((char **) *esp));
#endif
          }
          
          /* Save the last character */
          t = *(char *) addr;
        }


        /* Push pointer to the start addr of the array elem argv[0] */
        addr = (uint32_t) *esp;
        *esp = (void *) (((uint32_t) *esp) - sizeof (char **));
        *((char ***) *esp) = (char **) addr; 
#ifdef CSLDEBUG
        printf("0x%8X: points to argv[0], 0x%X\n", (uint32_t) *esp, *((uint32_t *) *esp));
#endif 

        /* Push argc */
        *esp = (void *) (((uint32_t) *esp) - sizeof (int));
        *((int *) *esp) = (int) argc;
#ifdef CSLDEBUG
        printf("0x%8X: points to argc, %d\n", (uint32_t) *esp, *((int *) *esp));
#endif 

        /* Push fake return addr 0 */
        *esp = (void *) (((uint32_t) *esp) - sizeof (void *));
        *((void **) *esp) = 0;
#ifdef CSLDEBUG
        printf("0x%8X: points to ret, 0x%X\n", (uint32_t) *esp, *((uint32_t *) *esp));
#endif 
      }
      else
        virtualmemory_frame_free(kpage);//page free 함수 대신 우리가 만든 가상메모리 free 함수를 사용하였다.
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)//가장 중요한 핵심이 되는 install_page 함수이다. 원래는 페이지 디렉토리(페이지 테이블)에만
{                                                     //함수를 사용하여 엔트리를 추가했지만, 페이지 테이블에 들어가는 내용은
  struct thread *t = thread_current ();               //반드시 가상 메모리에도 같은 내용으로 복사되어 존재해야되기 때문에 아래와 같이
                                                      //가상메모리 테이블도 함께 install frame해 주었다.
  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  bool success = (pagedir_get_page (t->pagedir, upage) == NULL);
  success = success && pagedir_set_page (t->pagedir, upage, kpage, writable);

#ifdef VM //가상 메모리가 define 되있을 경우만 실행한다.
  printf("TRYING\n");

  success = success && virtualmemory_table_install_frame(t->table,upage,kpage);//success를 가상메모리 설치 함수와도 AND연산하였다.
  if(success) virtualmemory_frame_unpin(kpage);//가상 메모리를 설치하면 처음에는 pinned 상태가 true이기 때문에 unpin해 주었다.

  #endif

  return success;//가상메모리 테이블까지 설치 완료

}
