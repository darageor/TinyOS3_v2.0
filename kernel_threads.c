
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"
#include "util.h"

//edited for part 1.1
void start_thread()
{
  PTCB* cur_ptcb = cur_thread()->ptcb;
  int exitval;

  Task call =  cur_ptcb->task;
  int argl = cur_ptcb->argl;
  void* args = cur_ptcb->args;

  exitval = call(argl,args);
  assert(cur_ptcb != NULL);
  ThreadExit(exitval);
}

PTCB* search_ptcb(Tid_t tid)
{   
  PTCB* ptcb = (PTCB*)tid;
  if(rlist_find(&CURPROC->ptcb_list, ptcb, NULL))
    return ptcb;
  else
   return NULL;
}

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{

  if(task != NULL) {
    PCB* curproc = CURPROC;
    curproc->thread_count ++; 
    PTCB* cur_ptcb = spawn_ptcb(curproc,task,argl,args);
    cur_ptcb->tcb = spawn_thread(curproc, start_thread);
    
    cur_ptcb->tcb->owner_pcb = curproc;
    cur_ptcb->tcb->ptcb = cur_ptcb;
    wakeup(cur_ptcb->tcb);
    return (Tid_t) cur_ptcb;
  }

	return NOTHREAD;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->ptcb;
}

/**
  @brief Join the given thread.
*/
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
  PTCB* ptcb_to_join = search_ptcb(tid);

  if(ptcb_to_join == NULL) return -1;

  if(ptcb_to_join->tcb == NULL || ptcb_to_join->tcb == cur_thread()) return -1;


  if((ptcb_to_join->detached == 1)) return -1;             

      rcinc(ptcb_to_join);

    while(ptcb_to_join->exited!=1 && ptcb_to_join->detached != 1 ){// check if stopped maybe 
      kernel_wait(&ptcb_to_join->exit_cv,SCHED_USER);
    }

    if(ptcb_to_join->detached==1){
      rcdec(ptcb_to_join);
      return -1 ;
    }

    if(exitval!=NULL) 
    *exitval=ptcb_to_join->exitval;

    rcdec(ptcb_to_join);
  
    return 0;
  //no reason to go on
  
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  PTCB* item = search_ptcb(tid);
  int ret = -1;
  if(item != NULL) {   //&& get_pid(CURPROC) != 1)     
    if(item->exited == 0){

      item->detached = 1;

      ret = 0;
    } 
    kernel_broadcast(&item->exit_cv);
  }
  return ret;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

  PTCB* ptcb = cur_thread()->ptcb;
  
  ptcb->exitval = exitval;
  ptcb->exited = 1;
  kernel_broadcast(&ptcb->exit_cv);


  PCB *curproc = CURPROC;  /* cache for efficiency */
  curproc->thread_count --;

  if(curproc->thread_count == 0 ){


    if(get_pid(curproc)!=1) {

      /* Reparent any children of the exiting process to the 
         initial task */
      PCB* initpcb = get_pcb(1);
      while(!is_rlist_empty(& curproc->children_list)) {
        rlnode* child = rlist_pop_front(& curproc->children_list);
        child->pcb->parent = initpcb;
        rlist_push_front(& initpcb->children_list, child);
      }


      /* Add exited children to the initial task's exited list 
         and signal the initial task */
      if(!is_rlist_empty(& curproc->exited_list)) {
        rlist_append(& initpcb->exited_list, &curproc->exited_list);
        kernel_broadcast(& initpcb->child_exit);
      }

      /* Put me into my parent's exited list */
      rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
      kernel_broadcast(& curproc->parent->child_exit);
    }

    /* Release the args data */
    if(curproc->args) {
      free(curproc->args);
      curproc->args = NULL;
    }

    /* Clean up FIDT */
    for(int i=0;i<MAX_FILEID;i++) {
      if(curproc->FIDT[i] != NULL) {
        FCB_decref(curproc->FIDT[i]);
        curproc->FIDT[i] = NULL;
      }
    }


    assert(is_rlist_empty(& curproc->children_list));
    assert(is_rlist_empty(& curproc->exited_list));

    /* Clean the remaining ptcb's */
    while(!is_rlist_empty(& curproc->ptcb_list)) {
        rlnode* rl_ptcb = rlist_pop_front(& curproc->ptcb_list);
        if(rl_ptcb->ptcb->refcount==0){
            rlist_remove(&rl_ptcb->ptcb->ptcb_list_node);
            free(rl_ptcb->ptcb);
    }
      }

    /* Disconnect my main_thread */
    curproc->main_thread = NULL;
    
    /* Now, mark the process as exited. */
    curproc->pstate = ZOMBIE;
  }


  /* Bye-bye cruel world */
  kernel_sleep(EXITED, SCHED_USER);
}

/**
  @brief Increases the ref_count of the current thread.
  */
void rcinc(PTCB* ptcb)
{
  ptcb->refcount ++;
}

/**
  @brief Decreases the ref_count of the current thread.
         If the counter is 0 after the decrease then it makes the ptcb free.
  */
void rcdec(PTCB* ptcb)
{
  ptcb->refcount --;

  if(ptcb->refcount==0){
      rlist_remove(&ptcb->ptcb_list_node);
      free(ptcb);
    }
}
