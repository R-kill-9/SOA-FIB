/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>
#include <errno.h>
#include <io.h>



#include <mm.h>
#include <mm_address.h>

#include <sched.h>

#define LECTURA 0
#define ESCRIPTURA 1



int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
  return 0;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}

int sys_getpid()
{
	return current()->PID;
}

int child_PID=1000;


int ret_from_fork(){
  return 0;
}

int sys_fork()
{
  int PID=-1;

  if (list_empty(&free_queue))
    return -ENOMEM;

  struct list_head *free_queue_head = list_first(&free_queue);
  list_del(free_queue_head);

  struct task_struct *child_task_s = list_head_to_task_struct(free_queue_head);
  union task_union *child_task_u = (union task_union *)child_task_s;

  struct task_struct *parent_task_s = current();
  union task_union *parent_task_u = (union task_union *)parent_task_s;

  copy_data(parent_task_u, child_task_u, sizeof(union task_union));

  allocate_DIR(child_task_s);

  page_table_entry *child_PT = get_PT(child_task_s);

  int page = 0;
  int new_frame;

  for (page = 0; page < NUM_PAG_DATA; ++page)
  {
    new_frame = alloc_frame();
    if (new_frame != -1)
    {
      set_ss_pag(child_PT, PAG_LOG_INIT_DATA + page, new_frame);
    }
    else // dealocatar todo
    {
      // hacemos el bucle hacia atrás
      for (; page >= 0; --page)
      {
        free_frame(get_frame(child_PT, PAG_LOG_INIT_DATA + page));
        del_ss_pag(child_PT, PAG_LOG_INIT_DATA + page);
      }
      list_add_tail(free_queue_head, &free_queue);
      return -EAGAIN;
    }
  }

  page_table_entry *parent_PT = get_PT(parent_task_s);

  for (page = 0; page < NUM_PAG_KERNEL; ++page)
  {
    set_ss_pag(child_PT, page, get_frame(parent_PT, page));
  }

  for (page = 0; page < NUM_PAG_CODE; ++page)
  {
    set_ss_pag(child_PT, PAG_LOG_INIT_CODE + page,
               get_frame(parent_PT, PAG_LOG_INIT_CODE + page));
  }

  for (page = 0; page < NUM_PAG_DATA; ++page)
  {
    set_ss_pag(parent_PT, TOTAL_PAGES - NUM_PAG_DATA + page,
               get_frame(child_PT, PAG_LOG_INIT_DATA + page));

    copy_data((void *)((PAG_LOG_INIT_DATA + page) << 12),
              (void *)((TOTAL_PAGES - NUM_PAG_DATA + page) << 12),
              PAGE_SIZE);

    del_ss_pag(parent_PT, TOTAL_PAGES - NUM_PAG_DATA + page);
  }

  set_cr3(get_DIR(parent_task_s));

  PID = child_PID++;

  child_task_s->PID = PID;
  child_task_s->state = ST_READY;
  init_stats(&child_task_s->stats);
  

  child_task_u->stack[KERNEL_STACK_SIZE - 18] = (unsigned long)&ret_from_fork;
  child_task_u->stack[KERNEL_STACK_SIZE - 19] = 0;
  child_task_s->kernel_esp=(unsigned long)&child_task_u->stack[KERNEL_STACK_SIZE - 19];

  list_add_tail(&child_task_s->anchor, &ready_queue);

  return PID;
}

void sys_exit()
{  
  int i;

  page_table_entry *process_PT = get_PT(current());

  // Deallocate all the propietary physical pages
  for (i=0; i<NUM_PAG_DATA; i++)
  {
    free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
    del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
  }
  
  /* Free task_struct */
  list_add_tail(&(current()->anchor), &free_queue);
  
  current()->PID=-1;
  
  /* Restarts execution of the next process */
  sched_next_rr();
}

char buff[4096]; //Este es el responsable de haber estado 5 dias estancado en el fork
int sys_write(int fd, char * buffer, int size){
  
  int err=check_fd(fd,ESCRIPTURA);
  if (err!=0) return err;
  
  if(buffer == NULL){
    return -EFAULT;
  }

  if (size<=0){
    return -EINVAL;
  }

  if (!access_ok(VERIFY_WRITE,buffer,size)){
    return -EFAULT;
  }
  //Comprobaciones ok
  
  int to_writte = size;
  int written=0;
  while(to_writte>4096) {
    copy_from_user(buffer,buff,4096);
   written= sys_write_console(buff,4096);
    to_writte-=4096;
  }
  if (to_writte){ //Aun queda por escribir pero menor que 4k
    copy_from_user(buffer,buff,to_writte);
    written=sys_write_console(buff,to_writte);
  }
  return written;
}


extern int zeos_ticks;
int sys_gettime(){
 
  return zeos_ticks;
}

void update_stats(unsigned long *v, unsigned long *elapsed)
{
  unsigned long current_ticks;

  current_ticks = get_ticks();

  *v += current_ticks - *elapsed;

  *elapsed = current_ticks;
}


void user_to_system(void)
{
  current()->stats.user_ticks += get_ticks()-current()->stats.elapsed_total_ticks;
  current()->stats.elapsed_total_ticks = get_ticks();

}

void system_to_user(void)
{
  current()->stats.system_ticks += get_ticks()-current()->stats.elapsed_total_ticks;
  current()->stats.elapsed_total_ticks = get_ticks();

}

void system_to_ready(void)
{
  current()->stats.system_ticks += get_ticks()-current()->stats.elapsed_total_ticks;
  current()->stats.elapsed_total_ticks = get_ticks();
}

void ready_to_system(void)
{
  current()->stats.ready_ticks += get_ticks()-current()->stats.elapsed_total_ticks;
  current()->stats.elapsed_total_ticks = get_ticks();
}


extern int quantum_left;

int sys_get_stats(int pid, struct stats *st)
{
  int i;

  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))){
  printk("No Entra 1");
    return -EFAULT;
  }

  if (pid < 0)
    return -EINVAL;
  for (i = 0; i < NR_TASKS; i++)
  {
    if (task[i].task.PID == pid)
    {
        task[i].task.stats.remaining_ticks = quantum_left;
        copy_to_user(&(task[i].task.stats), st, sizeof(struct stats));

      return 0;
    }
  }
  printk("Entra 3");
  return -ESRCH; /*ESRCH */
}