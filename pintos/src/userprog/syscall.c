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

//#define VADDR_BOTTOM ((void *) 0x08048000)

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
int user_kernel(const void *vaddr);
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
	int arg[3];
	check_validity((const void*) f->esp);
	switch (* (int *) f->esp)
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			get_arguement(f, &arg[0],1);
			exit(arg[0]);
			break;
		case SYS_EXEC:
			get_arguement(f, &arg[0], 1);
			arg[0] = user_kernel((const void *) arg[0]);
			f->eax = exec((const char *) arg[0]);
			break;
		case SYS_WAIT:
			get_arguement(f, &arg[0],1);
			f->eax =wait(arg[0]);
			break;
		case SYS_CREATE:
			get_arguement(f, &arg[0],2);
			arg[0] = user_kernel((const void *) arg[0]);
			f->eax = create((const char *) arg[0], (unsigned) arg[1]);
			break;
		case SYS_REMOVE:
			get_arguement(f, &arg[0],1);
			arg[0] = user_kernel((const void *) arg[0]);
			f->eax =remove((const char *) arg[0]);
			break;
		case SYS_OPEN:
			get_arguement(f, &arg[0],1);
			arg[0] = user_kernel((const void *) arg[0]);
			f->eax =open((const char *) arg[0]);
			break;
		case SYS_FILESIZE:
			get_arguement(f, &arg[0],1);
			f->eax =filesize(arg[0]);
			break;
		case SYS_READ:
			get_arguement(f, &arg[0],3);
			arg[1] = user_kernel((const void *) arg[1]);
			f->eax =read(arg[0], (void *) arg[1], (unsigned) arg[2]);
			break;
		case SYS_WRITE:
			get_arguement(f, &arg[0],3);
			arg[1] = user_kernel((const void *) arg[1]);
			f->eax = write(arg[0], (const void *) arg[1], (unsigned) arg[2]);
			break;
		case SYS_SEEK:
			get_arguement(f, &arg[0],2);
			seek(arg[0], (unsigned) arg[1]);
			break;
		case SYS_TELL:
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
	struct thread *current = thread_current();
	if(thread_alive(current->parent))
	{
		current->cp->status = status;
	}
	printf("%s: exit(%d)\n", current->name, status);
	thread_exit();
}
//---------------------------------
pid_t exec(const char *cmd_line)
{
	pid_t pid = process_execute(cmd_line);
	struct process_info* child_process = get_child_process(pid);
	ASSERT(child_process);
	if(child_process->load == 0)
	{
		sema_down(&child_process->load_semaphore);
	}
	if(child_process->load == 2)
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
	bool noerror = filesys_create(file, initial_size);
	lock_release(&file_lock);
	return noerror;
}
//---------------------------------
bool remove(const char *file)
{
	lock_acquire(&file_lock);
	bool noerror = filesys_remove(file);
	lock_release(&file_lock);
	return noerror;
}
//---------------------------------
int open(const char *file)
{
	lock_acquire(&file_lock);
	struct file *openfile = filesys_open(file);
	if(!openfile)
	{
		lock_release(&file_lock);
		return -1;
	}
	int fd = add_file(openfile);
	lock_release(&file_lock);
	return fd;
}
//---------------------------------
int filesize(int fd)
{
	lock_acquire(&file_lock);
	struct file *getsize = get_file(fd);
	if(!getsize)
	{
		lock_release(&file_lock);
		return -1;
	}
	int size = file_length(getsize);
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
	struct file *readfile = get_file(fd);
	if(!readfile)
	{
		lock_release(&file_lock);
		return -1;
	}
	int byte = file_read(readfile, buffer, size);
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
	struct file *writefile = get_file(fd);
	if(!writefile)
	{
		lock_release(&file_lock);
		return -1;
	}
	int bytes = file_write(writefile,buffer,size);
	lock_release(&file_lock);
	return bytes;
}
//---------------------------------
void seek(int fd, unsigned position)
{
	lock_acquire(&file_lock);
	struct file *seekfile = get_file(fd);
	if(!seekfile)
	{
		lock_release(&file_lock);
		return;
	}
	file_seek(seekfile, position);
	lock_release(&file_lock);
}
//---------------------------------
unsigned tell(int fd)
{
	lock_acquire(&file_lock);
	struct file *tellfile = get_file(fd);
	if(!tellfile)
	{
		lock_release(&file_lock);
		return -1;
	}
	off_t offset = file_tell(tellfile);
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
	if(!is_user_vaddr(vaddr) || vaddr < 0x08048000)
	{
		exit(-1);
	}
}
//---------------------------------
//ADDIDTIONAL FUCNTIONS
int user_kernel(const void *vaddr)
{
	check_validity(vaddr);	
	void *ukptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if(!ukptr)
	{
		exit(-1);
	}
	return (int) ukptr;
}
int add_file(struct file *f)
{
	struct file_proc *addthis = malloc(sizeof(struct file_proc));
	addthis->file = f;
	addthis->fd = thread_current()->fd;
	thread_current()->fd++;
	list_push_back(&thread_current()->filelist, &addthis->elem);
	return addthis->fd;
}
struct file* get_file(int fd)
{
	struct thread *getthis_thread = thread_current();
	struct list_elem *i;
	for(i = list_begin(&getthis_thread->filelist); 
		i != list_end(&getthis_thread->filelist); 
		i= list_next(i))
	{
		struct file_proc *fp = list_entry(i, struct file_proc, elem);
		if(fd == fp->fd)
			return fp->file;
	}
	return NULL;
}
void close_file(int fd)
{
	struct thread *current = thread_current();
	struct list_elem *next, *e = list_begin(&current->filelist);
	while(e != list_end(&current->filelist))
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
	struct thread *current = thread_current();
	struct list_elem *e;
	for(e = list_begin (&current->list_of_children);
		e != list_end(&current->list_of_children);
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
	struct process_info* child = malloc(sizeof(struct process_info));
	child->pid = pid;
	child->load = 0;
	child->wait = false;
	child->exit = false;
	sema_init(&child->load_semaphore,0);
	sema_init(&child->exit_semaphore,0);
	list_push_back(&thread_current()->list_of_children, &child->elem);
	return child;
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
