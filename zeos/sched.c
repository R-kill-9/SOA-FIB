/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

union task_union task[NR_TASKS]
  __attribute__((__section__(".data.task")));

#if 1
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, anchor); // he cambiado list a anchor
}
#endif

struct list_head free_queue;
struct list_head ready_queue;
extern struct list_head blocked;

struct task_struct *idle_task = NULL;

void stack_swap();
void writeMSR();

int quantum_left = 0;

/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t)
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t)
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t)
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos];

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

void init_idle (void)
{
  struct list_head *free_queue_head = list_first(&free_queue);
  list_del(free_queue_head);

  struct task_struct *idle_task_s = list_head_to_task_struct(free_queue_head);

  // crea un puntero a la union idle_task_u
  // que se inicializa con la dirección de memoria de la
  // variable idle_task_s
  union task_union *idle_task_u = (union task_union *)idle_task_s;

  idle_task_s->PID = 0;
  idle_task_s->quantum = QUANTUM;

  init_stats(&idle_task_s->stats);
  allocate_DIR(idle_task_s);

  // dirección de retorno
  idle_task_u->stack[KERNEL_STACK_SIZE - 1] = (unsigned long)&cpu_idle;
  // ebp
  idle_task_u->stack[KERNEL_STACK_SIZE - 2] = 0;

  idle_task_s->kernel_esp = (unsigned long)&idle_task_u->stack[KERNEL_STACK_SIZE - 2];

  // puntero global a idle_task
  idle_task = idle_task_s;
}

void init_task1(void)
{
  struct list_head *free_queue_head = list_first(&free_queue);
  list_del(free_queue_head);

  struct task_struct *init_task_s = list_head_to_task_struct(free_queue_head);

  // crea un puntero a la union idle_task_u

  // variable idle_task_s
  union task_union *init_task_u = (union task_union *)init_task_s;

  init_task_s->PID = 1;

  init_task_s->state = ST_RUN;
  init_task_s->quantum = QUANTUM;
  quantum_left = init_task_s->quantum;

  init_stats(&init_task_s->stats);

  allocate_DIR(init_task_s); // allocate dir
  set_user_pages(init_task_s); // initializate adress space

  tss.esp0 = (unsigned long)&(init_task_u->stack[KERNEL_STACK_SIZE]);
  writeMSR(0x175, 0, (unsigned long)&(init_task_u->stack[KERNEL_STACK_SIZE]));

  set_cr3(get_DIR(init_task_s));
}

void init_free_queue()
{
  INIT_LIST_HEAD(&free_queue);
  int i;
  for (i = 0; i < NR_TASKS; ++i)
  {
    list_add_tail(&(task[i].task.anchor), &free_queue);
  }
}

void init_sched()
{
  init_free_queue();
  INIT_LIST_HEAD(&ready_queue);
}

void init_stats(struct stats *s)
{
  s->user_ticks = 0;
  s->system_ticks = 0;
  s->blocked_ticks = 0;
  s->ready_ticks = 0;
  s->elapsed_total_ticks = get_ticks();
  s->total_trans = 0;
  s->remaining_ticks = get_ticks();


}


struct task_struct* current()
{
  int ret_value;

  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}

void inner_task_switch(union task_union *new)
{
  tss.esp0 = (unsigned long)&(new->stack[KERNEL_STACK_SIZE]);
  writeMSR(0x175, 0, (unsigned long)&(new->stack[KERNEL_STACK_SIZE]));

  set_cr3(get_DIR(&new->task)); // page table entry de new

  stack_swap(&current()->kernel_esp, (unsigned long)new->task.kernel_esp);
}

void sched_next_rr()
{
  struct task_struct *new;
  if (!list_empty(&ready_queue))
  {
    struct list_head *new_head = list_first(&ready_queue);
    new = list_head_to_task_struct(new_head);
    update_process_state_rr(new, NULL);
  }
  else 
    new=idle_task;
  
  new->state=ST_RUN;

  unsigned long ticks=get_ticks();

  current()->stats.system_ticks += ticks - current()->stats.elapsed_total_ticks;
  current()->stats.elapsed_total_ticks = ticks;

  new->stats.ready_ticks += ticks - new->stats.elapsed_total_ticks;
  new->stats.elapsed_total_ticks = ticks;

  new->stats.total_trans++;

  quantum_left = new->quantum;
  task_switch((union task_union *)new);
}


void update_process_state_rr(struct task_struct *t, struct list_head *dest)
{
  if (t->state != ST_RUN)
    list_del(&t->anchor);

  if (dest != NULL)
  {
    list_add_tail(&t->anchor, dest);
    if (dest == &ready_queue)
    {
      
      unsigned long ticks=get_ticks();
      t->stats.system_ticks += ticks - t->stats.elapsed_total_ticks;
      t->stats.elapsed_total_ticks= ticks;
      
      t->state = ST_READY;
    }
    else
      t->state = ST_BLOCKED;
  }
  else
    t->state = ST_RUN;
}

int needs_sched_rr()
{
  if (quantum_left == 0 && !list_empty(&ready_queue))
    return 1;
  if (quantum_left == 0)
    quantum_left = get_quantum(current());

  return 0;
}

void update_sched_data_rr()
{
  --quantum_left;
  if (quantum_left < 0) quantum_left = 0;
}

void schedule() {
  update_sched_data_rr();
  if (needs_sched_rr()) {
    sched_next_rr();
  }
}

int get_quantum(struct task_struct *t)
{
  return t->quantum;
}

void set_quantum(struct task_struct *t, int new_quantum)
{
  t->quantum = new_quantum;
}