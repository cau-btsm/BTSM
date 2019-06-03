#include "userprog/syscall.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"

#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>

#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"

#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/file.h"

#include "devices/shutdown.h"
#include "devices/input.h"


struct lock file_lock;

static void syscall_handler (struct intr_frame *);
static void halt (void);
void exit (int);
static tid_t exec (const char *);
static int wait (tid_t);
static bool create (const char *, unsigned);
static bool remove (const char *);
static int open (const char *);
static int filesize (int);
static int read (int, void *, unsigned);
static int write (int, const void *, unsigned);
static void seek (int, unsigned);
static unsigned tell (int);
static void close (int);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

static inline void
check_address (void *addr)
{
  if (!(is_user_vaddr (addr) && addr >= (void *)0x08048000UL))
    exit (-1);
}

static inline void
check_address4 (void *addr)
{
  check_address (addr);
  check_address (addr + 3);
}

static inline void
get_arguments (void *esp, void *args, int count)
{
  ASSERT (1 <= count && count <= 4);
  while (count--)
  {
    check_address4 ((esp += 4));
    *((int32_t *) args) = *(int32_t *) esp;
    args += 4;
  }
}

static inline void
check_user_string_l (const char *str, unsigned size)
{
  while (size--)
    check_address ((void *) (str++));  
}

static inline void
check_user_string (const char *str)
{
  for (; check_address ((void *) str), *str; str++);
}

static inline char *
get_user_string_l (const char *str, unsigned size)
{
  char *buffer = 0;
  buffer = malloc (size);
  if (!buffer)
    return 0;
  memcpy (buffer, str, size);
  return buffer;
}

static inline char *
get_user_string (const char *str)
{
  unsigned size;
  char *buffer;
  size = strlen (str) + 1;
  buffer = get_user_string_l (str, size);
  return buffer; 
}

static inline void
free_single_user_string (char **args, int flag, int index)
{
  if (flag & (0b1000 >> index))
    {
      free (args[index]);
      args[index] = 0;
    }
}

static inline void
free_user_strings (char **args, int flag)
{
  ASSERT (0 <= flag && flag <= 0b1111);
  free_single_user_string (args, flag, 0);
  free_single_user_string (args, flag, 1);
  free_single_user_string (args, flag, 2);
  free_single_user_string (args, flag, 3);
}

static inline void
get_single_user_string (char **args, int flag, int index)
{
  if (flag & (0b1000 >> index))
    {
      args[index] = get_user_string (args[index]);
      if (!args[index])
        {
          free_user_strings (args, flag & (0b11110000 >> index));
          exit(-1);
        }
    }
}

static inline void
check_single_user_string (char **args, int flag, int index)
{
  if (flag & (0b1000 >> index))
    check_user_string (args[index]);
}

static inline void
get_user_strings (char **args, int flag)
{
  ASSERT (0 <= flag && flag <= 0b1111);
  check_single_user_string (args, flag, 0);
  check_single_user_string (args, flag, 1);
  check_single_user_string (args, flag, 2);
  check_single_user_string (args, flag, 3);
  get_single_user_string (args, flag, 0);
  get_single_user_string (args, flag, 1);
  get_single_user_string (args, flag, 2);
  get_single_user_string (args, flag, 3);
}


static void
halt (void)
{
  shutdown_power_off ();
}

void
exit (int status)
{
  thread_current ()->exit_status = status;
  printf ("process %s: exited with %d\n", thread_name (), status);
  thread_exit ();
}

static tid_t
exec (const char *file)
{
  tid_t tid;
  struct thread *child;

  if ((tid = process_execute (file)) == TID_ERROR)
    return TID_ERROR;

  child = thread_get_child (tid);
  ASSERT (child);

  sema_down (&child->load_sema);

  if (!child->load_succeeded)
    return TID_ERROR;

  return tid;
}

static int
wait (tid_t tid)
{
  return process_wait (tid);
}

static bool
create (const char *file, unsigned initial_size)
{
  return filesys_create (file, initial_size); 
}

static bool
remove (const char *file)
{
  return filesys_remove (file);
}

static int
open (const char *file)
{
  int result = -1;
  lock_acquire (&file_lock);
  result = process_add_file (filesys_open (file));
  lock_release (&file_lock);
  return result;
}

static int
filesize (int fd)
{
  struct file *f = process_get_file (fd);
  if (f == NULL)
    return -1;
	return file_length (f);
}

static int
read (int fd, void *buffer, unsigned size)
{
  struct file *f;
  lock_acquire (&file_lock);

  if (fd == STDIN_FILENO)
  {
    unsigned count = size;
    while (count--)
      *((char *)buffer++) = input_getc();
    lock_release (&file_lock);
    return size;
  }
  if ((f = process_get_file (fd)) == NULL)
    {
      lock_release (&file_lock);
      return -1;
    }
  size = file_read (f, buffer, size);
  lock_release (&file_lock);
  return size;
}

static int
write (int fd, const void *buffer, unsigned size)
{
  struct file *f;
  lock_acquire (&file_lock);
  if (fd == STDOUT_FILENO)
    {
      putbuf (buffer, size);
      lock_release (&file_lock);
      return size;  
    }
  if ((f = process_get_file (fd)) == NULL)
    {
      lock_release (&file_lock);
      return 0;
    }
  size = file_write (f, buffer, size);
  lock_release (&file_lock);
  return size;
}

static void
seek (int fd, unsigned position)
{
  struct file *f = process_get_file (fd);
  if (f == NULL)
    return;
  file_seek (f, position);  
}

static unsigned
tell (int fd)
{
  struct file *f = process_get_file (fd);
  if (f == NULL)
    exit (-1);
  return file_tell (f);
}

static void
close (int fd)
{ 
  process_close_file (fd);
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int32_t args[4];
  check_address4 (f->esp);

  switch (*(int *) f->esp)
    {
      case SYS_HALT:
        halt ();
        break;
      case SYS_EXIT:
        get_arguments (f->esp, args, 1);
        exit (args[0]);
        break;
      case SYS_CREATE:
        get_arguments (f->esp, args, 2);
        get_user_strings ((char **) args, 0b1000);
        f->eax = create ((const char *) args[0], args[1]);
        free_user_strings ((char **) args, 0b1000);
        break;
      case SYS_REMOVE:
        get_arguments (f->esp, args, 1);
        get_user_strings ((char **) args, 0b1000);
        f->eax = remove ((const char *) args[0]);
        free_user_strings ((char **) args, 0b1000); 
        break;
      case SYS_WRITE:
        get_arguments (f->esp, args, 3);
        check_user_string_l ((const char *) args[1], (unsigned) args[2]);
        args[1] = (int) get_user_string_l ((const char *) args[1], (unsigned) args[2]);
        f->eax = write ((int) args[0], (const void *) args[1], (unsigned) args[2]);
        free ((void *) args[1]);
        args[1] = 0;
        break;
      case SYS_EXEC:
        get_arguments (f->esp, args, 1);
        get_user_strings ((char **) args, 0b1000);
        f->eax = exec ((const char *) args[0]);
        free_user_strings ((char **) args, 0b1000);
        break;
      case SYS_WAIT:
        get_arguments (f->esp, args, 1);
        f->eax = wait ((tid_t) args[0]);
        break;
      case SYS_OPEN:
        get_arguments (f->esp, args, 1);
        get_user_strings ((char **) args, 0b1000);
        f->eax = open ((const char *) args[0]);
        free_user_strings ((char **) args, 0b1000);
        break;
      case SYS_FILESIZE:
        get_arguments (f->esp, args, 1);
        f->eax = filesize ((int) args[0]);
        break;
      case SYS_READ:
        get_arguments (f->esp, args, 3);
        check_user_string_l ((const char *) args[1], (unsigned) args[2]);
        f->eax = read ((int) args[0], (void *) args[1], (unsigned) args[2]);
        break;
      case SYS_SEEK:
        get_arguments (f->esp, args, 2);
        seek ((int) args[0], (unsigned) args[1]);
        break;
      case SYS_TELL:
        get_arguments (f->esp, args, 1);
        f->eax = tell ((int) args[0]);
        break;
      case SYS_CLOSE:
        get_arguments (f->esp, args, 1);
        close ((int) args[0]);
        break;
      default:
        exit(-1);
    }
}
