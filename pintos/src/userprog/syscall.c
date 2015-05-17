#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <user/syscall.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "filesys/file.c"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/syscall.h"

struct lock file_lock;
//used for all file system syscalls
struct file_proc
{
	struct file *file;
	int fd;
	struct list_elem elem;
};


int my_add_file(struct file *f);
struct file* my_get_file(int fd);
void my_close_file(int fd);
static void syscall_handler (struct intr_frame *);
int translate(const void *vaddr);


void
syscall_init (void) 
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
//  printf ("system call!\n");
//  thread_exit ();
  int i;
  int arg[3];
  for(i = 0; i < 3; i++)
  {
  	  arg[i] = * ((int *) f->esp + i);
  }
	switch(arg[0])
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(arg[1]);
			break;
		case SYS_EXEC:
			arg[1] = translate((const void *) arg[1]);
			f->eax =  exec((const char *) arg[1]);
			break;
		case SYS_WAIT:
			f->eax =wait(arg[1]);
			break;
		case SYS_CREATE:
			arg[1] = translate((const void *) arg[1]);
			f->eax =create((const char *)arg[1], (unsigned) arg[2]);
			break;
		case SYS_REMOVE:
			arg[1] = translate((const void *) arg[1]);
			f->eax =remove((const char *) arg[1]);
			break;
		case SYS_OPEN:
			arg[1] = translate((const void *) arg[1]);
			f->eax =open((const char *) arg[1]);
			break;
		case SYS_FILESIZE:
			f->eax =filesize(arg[1]);
			break;
		case SYS_READ:
			arg[2] = translate((const void *) arg[2]);
			f->eax =read(arg[1], (void *) arg[2], (unsigned) arg[3]);
			break;
		case SYS_WRITE:
			arg[2] = translate((const void *) arg[2]);
			f->eax =write(arg[1], (const void *) arg[2], (unsigned) arg[3]);
			break;
		case SYS_SEEK:
			seek(arg[1], (unsigned) arg[2]);
			break;
		case SYS_TELL:
			f->eax =tell(arg[1]);
			break;
		case SYS_CLOSE:
			close(arg[1]);
			break;
	}
}


void halt (void)
{
	shutdown_power_off();	
}
//---------------------------------
void exit(int status)
{
	thread_exit();
}
//---------------------------------
pid_t exec(const char *cmd_line)
{
	pid_t pid = process_execute(cmd_line);
	return pid;
}
//---------------------------------
int wait(pid_t pid)
{
	return process_wait(pid);
}
//---------------------------------
bool create(const char *file, unsigned initial_size)
{
	lock_acquire(&file_lock);
	bool success = filesys_create(file, initial_size);
	lock_release(&file_lock);
	return success;
}
//---------------------------------
bool remove(const char *file)
{
	lock_acquire(&file_lock);
	bool success = filesys_remove(file);
	lock_release(&file_lock);
	return success;
}
//---------------------------------
int open(const char *file)
{
	lock_acquire(&file_lock);
	struct file *f = filesys_open(file);
	if(!f)
	{
		lock_release(&file_lock);
		return -1;
	}
	int fd = my_add_file(f);
	lock_release(&file_lock);
	return fd;
}
//---------------------------------
int filesize(int fd)
{
	lock_acquire(&file_lock);
	struct file *f = my_get_file(fd);
	if(!f)
	{
		lock_release(&file_lock);
		return -1;
	}
	int size = file_length(f);
	lock_release(&file_lock);
	return size;
}
//---------------------------------
int read(int fd, void *buffer, unsigned size)
{
	if(fd == STDIN_FILENO)
	{
		unsigned i;
		uint8_t* buffer_local = (uint8_t *) buffer;
		for(i = 0; i < size; i++)
		{
			buffer_local[i] = input_getc();
		}
		return size;
	}
	lock_acquire(&file_lock);
	struct file *f = my_get_file(fd);
	if(!f)
	{
		lock_release(&file_lock);
		return -1;
	}
	int byte = file_read(f, buffer, size);
	lock_release(&file_lock);
	return byte;
}
//---------------------------------
int write(int fd, const void *buffer, unsigned size)
{
	if(fd == STDOUT_FILENO)
	{
		putbuf(buffer, size);
		return size;
	}
	lock_acquire(&file_lock);
	struct file *f = my_get_file(fd);
	if(!f)
	{
		lock_release(&file_lock);
		return -1;
	}
	int bytes = file_write(f,buffer,size);
	lock_release(&file_lock);
	return bytes;
}
//---------------------------------
void seek(int fd, unsigned position)
{
	lock_acquire(&file_lock);
	struct file *f = my_get_file(fd);
	if(!f)
	{
		lock_release(&file_lock);
		return;
	}
	file_seek(f, position);
	lock_release(&file_lock);
}
//---------------------------------
unsigned tell(int fd)
{
	lock_acquire(&file_lock);
	struct file *f = my_get_file(fd);
	if(!f)
	{
		lock_release(&file_lock);
		return -1;
	}
	off_t offset = file_tell(f);
	lock_release(&file_lock);
	return offset;
}
//---------------------------------
void close(int fd)
{
	lock_acquire(&file_lock);
	my_close_file(fd);
	lock_release(&file_lock);
}
//---------------------------------
int translate(const void *vaddr)
{
	if(!is_user_vaddr(vaddr))
	{
		thread_exit();
		return 0;
	}
	void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if(!ptr)
	{
		thread_exit();
		return 0;
	}
	return (int) ptr;
}
int my_add_file(struct file *f)
{
	struct file_proc *fp = malloc(sizeof(struct file_proc));
	fp->file = f;
	fp->fd = thread_current()->fd;
	thread_current()->fd++;
	list_push_back(&thread_current()->filelist, &fp->elem);
	return fp->fd;
}
struct file* my_get_file(int fd)
{
	struct thread *t = thread_current();
	struct list_elem *i;
	for(i = list_begin(&t->filelist); 
		i != list_end(&t->filelist); 
		i= list_next(i))
	{
		struct file_proc *pf = list_entry(i, struct file_proc, elem);
		if(fd == pf->fd)
			return pf->file;
	}
	return NULL;
}
void my_close_file(int fd)
{
	struct thread *t = thread_current();
	struct list_elem *next, *e = list_begin(&t->filelist);
	while(e != list_end(&t->filelist))
	{
		next = list_next(e);
		struct file_proc *pf = list_entry(e, struct file_proc, elem);
		if(fd == pf->fd || fd == -1)
		{
			file_close(pf->file);
			list_remove(&pf->elem);
			free(pf);
			if(fd != -1)
			{
				return;
			}
		}
	e = next;
	}
}
