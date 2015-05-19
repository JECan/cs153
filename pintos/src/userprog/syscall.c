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
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"

#define VADDR_BOTTOM ((void *) 0x08048000)

struct lock file_lock;
//used for all file system syscalls
struct file_proc
{
	struct file *file;
	int fd;
	struct list_elem elem;
};



//SYSCALL FUNCTIONS
void halt(void);
void exit(int status);
pid_t exec(const char *cmd_line);
int wait(pid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
//END OF SYSCALL FUNCTIONS

int add_file(struct file *f);
struct file* get_file(int fd);
void close_file(int fd);
static void syscall_handler (struct intr_frame *);
int translate(const void *vaddr);
void get_arguement(struct intr_frame *f, int *arg, int n);
void check_validity(const void *vaddr);
struct process_info* get_child_process(int pid);
void remove_child_process(struct process_info *cp);

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
/*int i;
  int arg[4];
  for(i = 0; i < 4; i++)
  {
  	  arg[i] = * ((int *) f->esp + i);
  }
	switch(arg[0])
*/
	int arg[3];
	check_validity((const void*) f->esp);
	switch (* (int *) f->esp)
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			//exit(arg[1]);
			get_arguement(f, &arg[0],1);
			exit(arg[0]);
			break;
		case SYS_EXEC:
			//arg[1] = translate((const void *) arg[1]);
			//f->eax =  exec((const char *) arg[1]);
			get_arguement(f, &arg[0], 1);
			arg[0] = translate((const void *) arg[0]);
			f->eax = exec((const char *) arg[0]);
			break;
		case SYS_WAIT:
			get_arguement(f, &arg[0],1);
			f->eax =wait(arg[0]);
			break;
		case SYS_CREATE:
			//arg[1] = translate((const void *) arg[1]);
			//f->eax =create((const char *)arg[1], (unsigned) arg[2]);
			get_arguement(f, &arg[0],2);
			arg[0] = translate((const void *) arg[0]);
			f->eax = create((const char *) arg[0], (unsigned) arg[1]);
			break;
		case SYS_REMOVE:
			//arg[1] = translate((const void *) arg[1]);
			//f->eax =remove((const char *) arg[1]);
			get_arguement(f, &arg[0],1);
			arg[0] = translate((const void *) arg[0]);
			f->eax =remove((const char *) arg[0]);
			break;
		case SYS_OPEN:
			//arg[1] = translate((const void *) arg[1]);
			//f->eax =open((const char *) arg[1]);
			get_arguement(f, &arg[0],1);
			arg[0] = translate((const void *) arg[0]);
			f->eax =open((const char *) arg[0]);
			break;
		case SYS_FILESIZE:
			get_arguement(f, &arg[0],1);
			f->eax =filesize(arg[0]);
			break;
		case SYS_READ:
			//arg[2] = translate((const void *) arg[2]);
			//f->eax =read(arg[1], (void *) arg[2], (unsigned) arg[3]);
			get_arguement(f, &arg[0],3);
			arg[1] = translate((const void *) arg[1]);
			f->eax =read(arg[0], (void *) arg[1], (unsigned) arg[2]);
			break;
		case SYS_WRITE:
			//arg[2] = translate((const void *) arg[2]);
			//f->eax =write(arg[1], (const void *) arg[2], (unsigned) arg[3]);
			get_arguement(f, &arg[0],3);
			arg[1] = translate((const void *) arg[1]);
			f->eax = write(arg[0], (const void *) arg[1], (unsigned) arg[2]);
			break;
		case SYS_SEEK:
			//seek(arg[1], (unsigned) arg[2]);
			get_arguement(f, &arg[0],2);
			seek(arg[0], (unsigned) arg[1]);
			break;
		case SYS_TELL:
			//f->eax =tell(arg[1]);
			get_arguement(f, &arg[0], 1);
			f->eax = tell(arg[0]);
			break;
		case SYS_CLOSE:
			get_arguement(f,&arg[0],1);
			close(arg[0]);
			break;
	}
}

//---------------------------------
void halt (void)
{
	shutdown_power_off();	
}
//---------------------------------
void exit(int status)
{
//	thread_exit();
/*	struct thread *parent = thread_exists(thread_current()->parent);
	if(parent)
	{
		struct process_info *cp = get_child_process(thread_current()->tid);
		if(cp->wait)
		{
			cp->status = status;
		}
	}
	*/
	struct thread *current = thread_current();
	if(thread_alive(current->parent))
	{
		current->cp->status = status;
	}
	printf("%s: exit(%d)\n", current->name,status);
	thread_exit();
}
//---------------------------------
pid_t exec(const char *cmd_line)
{
	pid_t pid = process_execute(cmd_line);
	struct process_info* cp = get_child_process(pid);
	ASSERT(cp);
/*	if(!cp)
	{
		return -1;
	}
	ASSERT(cp);
*/
	//while(cp->load == 0)
	if(cp->load == 0)
	{
		//block the trhead
		//barrier();
		sema_down(&cp->load_semaphore);
	}
	if(cp->load == 2)
	{
		return -1;
	}		
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
	int fd = add_file(f);
	lock_release(&file_lock);
	return fd;
}
//---------------------------------
int filesize(int fd)
{
	lock_acquire(&file_lock);
	struct file *f = get_file(fd);
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
	struct file *f = get_file(fd);
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
	struct file *f = get_file(fd);
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
	struct file *f = get_file(fd);
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
	struct file *f = get_file(fd);
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
	close_file(fd);
	lock_release(&file_lock);
}
//---------------------------------
void check_validity(const void *vaddr)
{
	if(!is_user_vaddr(vaddr) || vaddr < VADDR_BOTTOM)
	{
		exit(-1);
	}
}
//---------------------------------

int translate(const void *vaddr)
{
	check_validity(vaddr);	
	void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if(!ptr)
	{
		exit(-1);
	}
	return (int) ptr;
}
int add_file(struct file *f)
{
	struct file_proc *fp = malloc(sizeof(struct file_proc));
	fp->file = f;
	fp->fd = thread_current()->fd;
	thread_current()->fd++;
	list_push_back(&thread_current()->filelist, &fp->elem);
	return fp->fd;
}
struct file* get_file(int fd)
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
void close_file(int fd)
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
//---------------------------------
struct process_info* get_child_process(int pid)
{
	struct thread *t = thread_current();
	struct list_elem *e;
	for(e = list_begin (&t->list_of_children);
		e != list_end(&t->list_of_children);
		e = list_next(e))
	{
		struct process_info *cp = list_entry (e, struct process_info, elem);
		if(pid == cp->pid)
		{
			return cp;
		}
	}
	return NULL;
}
//---------------------------------
void remove_child_process(struct process_info *cp)
{
	list_remove(&cp->elem);
	free(cp);
}
//---------------------------------
struct process_info* add_child(int pid)
{
	struct process_info* cp = malloc(sizeof(struct process_info));
	cp->pid = pid;
	cp->load = 0;
	cp->wait = false;
	cp->exit = false;
	//lock_init(&cp->lock_wait);
	sema_init(&cp->load_semaphore,0);
	sema_init(&cp->exit_semaphore,0);
	list_push_back(&thread_current()->list_of_children, &cp->elem);
	return cp;
}
//---------------------------------
void remove_child_proc(void)
{
	struct thread *curr = thread_current();
	struct list_elem *next = list_begin(&curr->list_of_children);
	struct list_elem *i = list_begin(&curr->list_of_children);
	while(i != list_end(&curr->list_of_children))
	{
		next = list_next(i);
		struct process_info *cp = list_entry(i, struct process_info, elem);
		list_remove(&cp->elem);
		free(cp);
		i = next;
	}
}
//---------------------------------
void get_arguement(struct intr_frame *f, int *arg, int n)
{
	int i;
	int *ptr;
	for(i=0; i < n; i++)
	{
		ptr = (int *) f->esp + i + 1;
		check_validity((const void *) ptr);
		arg[i] = *ptr;
	}
}
//---------------------------------
