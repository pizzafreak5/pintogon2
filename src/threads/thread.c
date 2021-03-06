#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

//Garrett Starts
/* 64 priority */
static struct list ready_lists[64];
//Garrett End

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);


fixed load_avg;    //the load average


/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  
  //For milfy G-Stack
  load_avg = convertToFixed(0);
  

  //Garrett Start
  int i = 0;
  for (i = 0; i < 64; i++)
  {
    list_init(&(ready_lists[i]));
  }
  //Garrett End

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
    
  //G ===========================
  //BSD STUFF
  //Check if BSD is on
  if (thread_mlfqs)
  {
    enum intr_level old_level = intr_disable();
    
        //Check if it has been a second
	  if (timer_ticks() % 100 == 0 )
	  {  		
	    
	  	//Update load
	  	int threads_ready = 0;
  
      if (thread_current() != idle_thread)
        threads_ready = 1;
  
      struct list_elem * acursor = NULL;
      
      for (acursor = list_begin(&all_list); acursor != list_end(&all_list); acursor = list_next(acursor))
      {
        if (list_entry(acursor, struct thread, allelem)->status == THREAD_READY)
        {
          threads_ready++;
        }
      }
      
      load_avg = addFixed( mulFixed(fractionInt(59,60), load_avg) , mulFixedInt(fractionInt(1,60),threads_ready));
      
      
      
      
	  	//Update recent_cpu and yield thread if priority is changed
	  	
      //recent_cpu = (2x load_avg) / (2xload_avg + 1) * recent_cpu + nice
      struct thread * temp;
      struct list_elem * cursor;
      
      fixed coef;
      fixed multi;
      
      for (cursor = list_begin(&all_list); cursor != list_end(&all_list); cursor = list_next(cursor))
      {
        temp = list_entry(cursor, struct thread, allelem);
        
        if (temp == thread_current())
        {
          //temp->recent_cpu = addFixedInt(temp->recent_cpu, 1);
        }
        else
        {
          coef = divFixed(mulFixedInt(load_avg, 2),addFixedInt(mulFixedInt(load_avg, 2), 1)	);
          multi = mulFixed(coef, temp->recent_cpu);
          temp->recent_cpu = addFixedInt(multi,temp->nice); 
        }
          
        
      }
	  	
	  }
	  
	  
	  if (timer_ticks() % 4 == 0)
    {
      thread_set_nice(t->nice);
      
      //Check to see if it the highest priority if not yield
      int i = 0;
      for (i = 63; i >= 0; i--)
      {
       //Find the highest priority list with elements
        if (!list_empty(&ready_lists[i]))
        {
          //NOT THE HIGHEST PRIORITY, YIELD
     	   if (i > thread_priority(t))
	        {
	          intr_set_level(old_level);
	          intr_yield_on_return ();
	        }
        }
      }
      
    }
	  
	  intr_set_level(old_level);
  }
  
  //G ==============================
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  //More garrett
  //Check if thread is higher priority, if so, yield currently
  struct thread * cur = thread_current(); //<- Garrett Insert

  if (cur != NULL && thread_priority(cur) < thread_priority(t))
  {

    thread_yield();
  }
  //End more Garrett

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();

  ASSERT (t->status == THREAD_BLOCKED);
  //Garrett Start
  //Push the thread on the back of its priority queue
  list_push_back (&(ready_lists[thread_priority(t)]), &t->elem);
  //Record the queue that thread is placed in
  t->in_queue = thread_priority(t); 
  //Garrett End

  /*list_push_back (&ready_list, &t->elem);*/ // Original line
  t->status = THREAD_READY;

  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
  {
    //Push the thread on the back of it's priority queue
    list_push_back (&(ready_lists[thread_priority(cur)]), &cur->elem);
    //list_push_back (&ready_list, &cur->elem); //ORIGINAL LINE
    //Record the queue that thread is placed in
    cur->in_queue = thread_priority(cur);  
  }
  //=============================
  cur->status = THREAD_READY; //Make sure this doesent get removed again
  //=============================
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{

  //if mlfq ignore call
  if (thread_mlfqs)
    return;
    
  if (new_priority <= PRI_MAX)
    thread_current ()->priority = new_priority;
  else
  {
    thread_current ()->priority = PRI_MAX;
  }
  //Garrett Start
  thread_yield();
  //Garrett end
}

//Garrett
//Function returns the highest value in the priority chain
int thread_priority(struct thread * t) 
{
  ASSERT(is_thread(t));
  
  if (thread_mlfqs)
  {
    t->priority = roundFixed(t->milf_priority);
  
    if (t->priority > PRI_MAX)
    {
      t->priority = PRI_MAX;
    }
    else if (t->priority < PRI_MIN)
    {
      t->priority = PRI_MIN;
    }
    
    return t->priority;
  }
    
  
  if(list_empty(&t->donors))
  {
   
    return t->priority;
  }
  else
  {
      
      
      return thread_priority(list_entry((list_front(&t->donors)), struct thread, donor_e));
  }
}

void reposition_in_queue(struct thread * t) //resets a non-running thread's position in the queues
{
  ASSERT(is_thread(t));
  enum intr_level old_level = intr_disable();
  if (t->priority != thread_priority(t))
  {
    
    //Find and remove the thread from the 
    struct list_elem * cursor;
    cursor = list_begin(&ready_lists[t->in_queue]);
    while (list_entry (cursor, struct thread, elem) != t && cursor != list_end(&ready_lists[t->in_queue]))
    {
      cursor = list_next(cursor);
    }
    
    list_remove(cursor);
    
    //Insert into new queue
    int new_queue = thread_priority(t);
    list_push_back(&ready_lists[new_queue], &t->elem);
    t->in_queue = thread_priority(t);
  }
  intr_set_level(old_level);
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  
  return thread_priority(thread_current());
}

//G's ======================================
/* Sets the current thread's nice value to NICE. */
void thread_set_nice (int newNice) //was int nice UNUSED
{
  
  enum intr_level old = intr_disable();  
    
  struct thread * t = thread_current();
  //Upper and lower bounds for nice
  if (newNice > 20)
  {
	  newNice = 20;
  } 
  else if (newNice < -20)
  {
	  newNice = -20;
  }
  t->nice = newNice;
  //setting priority to priority = PRI_MAX - (recent_cpu/4) - (nice*2)  
  t->milf_priority = subFixed(
	convertToFixed(PRI_MAX),
	subFixed(
	  	divFixedInt(t->recent_cpu, 4),
	  	mulFixedInt(convertToFixed(t->nice), 2)
	  )
  );
  
  //Set real thread priority value
  t->priority = roundFixed(t->milf_priority);
  
  if (t->priority > PRI_MAX)
  {
    t->priority = PRI_MAX;
  }
  else if (t->priority < PRI_MIN)
  {
    t->priority = PRI_MIN;
  }
  
  
  
  intr_set_level(old);
  
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  struct thread * t = thread_current();
  return t->nice;
  //return 0;
}


/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{

  return roundFixed( mulFixedInt(load_avg, 100)); //100 x load_avg
  /* load avg is a running average that should be recalculated 
	load_avg = (59/60)*load_avg + (1/60)*ready_threads
  */
  //load_avg is declared in .h with initialization to 0
  
  enum intr_level old_level = intr_disable();
  
  
  
  //fixed value = mulFixedInt(load_avg, 100);
  
  intr_set_level(old_level);
  
  
  
  
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  
  return roundFixed( mulFixedInt(thread_current()->recent_cpu, 100) ); //100 x recent_cpu
}
//G's ======================================

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the current thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  
  //Garrett
  if (!thread_mlfqs)
    t->priority = priority;
  //For BSD scheduler
  
  
  //G  =============
  t->nice = 0;
  t->milf_priority = convertToFixed(PRI_DEFAULT);
  t->recent_cpu = convertToFixed(0);
  
  if (!list_empty(&all_list) && thread_mlfqs)
  {
    t->recent_cpu = thread_current()->recent_cpu;
  }
  
  if (thread_mlfqs)
  {
     t->milf_priority = subFixed(
	    convertToFixed(PRI_MAX),
	    subFixed(
	    	divFixedInt(t->recent_cpu, 4),
	    	mulFixedInt(convertToFixed(t->nice), 2)
	    )
    );
  
    //Set real thread priority value
    t->priority = roundFixed(t->milf_priority);
  
    if (t->priority > PRI_MAX)
    {
      t->priority = PRI_MAX;
    }
    else if (t->priority < PRI_MIN)
    {
      t->priority = PRI_MIN;
    }
  }
  
  //G  =============
  
  //Garrett
  
  t->magic = THREAD_MAGIC;

  //Garrett: Initialize donor list, wanted
  list_init(&t->donors);
  t->waiting_on = NULL;
  
  

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  //Original Code
  /*
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
  */
  //Garrett Start
  /*This section of code implements priority scheduling*/
  int i = 0;
  for (i = 63; i >= 0; i--){
    //Find the highest priority list with elements
    if (!list_empty(&ready_lists[i]))
    {
      struct thread * next = list_entry (list_pop_front (&ready_lists[i]), struct thread, elem);
      //ASSERT(is_thread(next));
      //printf("\nNXT: Current: %s ", next->name);
      return next;
    }
  }

  //If nothing has been fount, return idle thread
  return idle_thread;
  //Garrett end
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  //Original Code
  
  //struct thread *cur = running_thread ();
  
  //ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  //cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  //thread_ticks = 0;

//#ifdef USERPROG
  /* Activate the new address space. */
  //process_activate ();
//#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  //if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    //{
      //ASSERT (prev != cur);
      //palloc_free_page (prev);
    //}
  

  //Garrett Start - Original seems fine, this doesn't need to handle re-scheduling the
  //                previous thread.
  struct thread *cur = running_thread();
  ASSERT(intr_get_level() == INTR_OFF);
  
  //Mark current thread as running_thread
  cur->status = THREAD_RUNNING;

  //Start a new time slice
  thread_ticks = 0;

  #ifdef USERPROG
    //Activate the new address space.
    process_activate ();
  #endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }

  
  //Garrett End
  

}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  //Original Code
  /*
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
  */
  //Garrett Start - Original code seems fine
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
  
  //Garrett End
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
