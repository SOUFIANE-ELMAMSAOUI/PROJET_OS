#include <stdlib.h>
#include "kernel.h"
#include "list.h"

#ifndef NULL
#define NULL 0
#endif


/*****************************************************************************
 * Global variables
 *****************************************************************************/

static uint32_t id=1;
Task * tsk_running = NULL;	/* pointer to ready task list : the first
                                     node is the running task descriptor */
Task * tsk_prev = NULL;
Task * tsk_sleeping = NULL;	/* pointer to sleeping task list */

/*****************************************************************************
 * SVC dispatch
 *****************************************************************************/
/* sys_add
 *   test function
 */
int sys_add(int a, int b)
{
    return a+b;
}

/* syscall_dispatch
 *   dispatch syscalls
 *   n      : syscall number
 *   args[] : array of the parameters (4 max)
 */
int32_t svc_dispatch(uint32_t n, uint32_t args[])
{
	int32_t result=-1;

    void *buffer;


    switch(n) {
        case 0:
            result = sys_add((int)args[0], (int)args[1]);
            break;
        case 1:  // os_alloc (malloc)
			buffer = malloc((size_t)args[0]);
			result = (buffer == NULL) ? -1 : (int32_t)buffer;
			break;
        case 2:  // os_free
            free((void*)args[0]);
            result = 0;
            break;
        case 3:  // os_start
            result = sys_os_start();
            break;
        case 4:  // task_new
            result = sys_task_new((TaskCode)args[0], args[1]);
            break;
        case 5:  // task_id
            result = sys_task_id();
            break;
        case 6:  // task_wait
            result = sys_task_wait(args[0]);
            break;
        case 7:  // task_kill
            result = sys_task_kill();
            break;
        case 8:  // sem_new
            result = (int32_t)sys_sem_new((int32_t)args[0]);
            break;
        case 9:  // sem_p
            result = sys_sem_p((Semaphore*)args[0]);
            break;
        case 10: // sem_v
            result = sys_sem_v((Semaphore*)args[0]);
            break;
    }
    return result;
}
void sys_switch_ctx()
{
	SCB->ICSR |= 1<<28; // set PendSV to pending
}
/*****************************************************************************
 * Round robin algorithm
 *****************************************************************************/
#define SYS_TICK  10

uint32_t sys_tick_cnt=0;

/* tick_cb
 *   system tick callback: task switching, ...
 */
void sys_tick_cb()
{
    if (tsk_running && tsk_running->next != tsk_running) {
        tsk_prev = tsk_running;
        tsk_running->status = TASK_READY;
        tsk_running = tsk_running->next;
        tsk_running->status = TASK_RUNNING;
        sys_switch_ctx();
    }

    int size = list_size(tsk_sleeping);
    for(int i = 0; i < size; ++i)
    {
        if (tsk_sleeping) {
            tsk_sleeping->delay -= SYS_TICK;
            if (tsk_sleeping->delay <= 0)
            {
                tsk_sleeping->delay = 0;

                Task* tskDelay;
                tsk_sleeping = list_remove_head(tsk_sleeping, &tskDelay);
                tskDelay->status = TASK_READY;
                tsk_running = list_insert_tail(tsk_running, tskDelay);
            }
            else
            {
                tsk_sleeping = tsk_sleeping->next;
            }
        }
    }
}
void SysTick_Handler(void)
{
	sys_tick_cnt++;

	if (sys_tick_cnt == SYS_TICK) {
		sys_tick_cnt = 0;
		sys_tick_cb();
	}
}

/*****************************************************************************
 * General OS handling functions
 *****************************************************************************/

/* sys_os_start
 *   start the first created task
 */
int32_t sys_os_start()
{
	tsk_running->status = TASK_RUNNING;
	sys_switch_ctx();
    __set_BASEPRI(0);

    SysTick_Config(SystemCoreClock / 1000U);
    return 0;
}

/*****************************************************************************
 * Task handling functions
 *****************************************************************************/
void task_kill();

/* sys_task_new
 *   create a new task :
 *   func      : task code to be run
 *   stacksize : task stack size
 *
 *   Stack frame:
 *      |    xPSR    |
 *      |     PC     |
 *      |     LR     |
 *      |     R12    |    ^
 *      |     R3     |    ^
 *      |     R2     |    | @++
 *      |     R1     |
 *      |     R0     |
 *      +------------+
 *      |     R11    |
 *      |     R10    |
 *      |     R9     |
 *      |     R8     |
 *      |     R7     |
 *      |     R6     |
 *      |     R5     |
 *      |     R4     |
 *      +------------+
 *      | EXC_RETURN |
 *      |   CONTROL  | <- sp
 *      +------------+
 */
int32_t sys_task_new(TaskCode func, uint32_t stacksize)
{
	uint32_t size = stacksize>96 ? 8*(((stacksize-1)/8)+1) : 96;
	
	Task *new_tsk = (Task *)malloc(sizeof(Task) + size);
	if (new_tsk == NULL)
	    return -1;

    new_tsk->id = id++;
    new_tsk->status = TASK_READY;
    new_tsk->delay = 0;

    // Initialiser la pile
    //new_task->splim = (uint32_t *)((uint8_t *)new_task + sizeof(Task));
    new_tsk->splim = (uint32_t*)(new_tsk+1);
    new_tsk->sp = new_tsk->splim + (size / sizeof(uint32_t));

    // R0-R12
    // CTRL
    // EXC_RET
    // LR (return addr)
    // PC (task func addr)
    // xPSR

    new_tsk->sp -= 18;

    new_tsk->sp[0] = (0x0) | (0x1 << 0); // CTRL = unprivileged , voir page 8 doc 0
    new_tsk->sp[1] = 0xFFFFFFFD; // EXC_RET = thread, psp , voir page 14 doc 1
    new_tsk->sp[15] = (uint32_t)task_kill; // LR = return address
    new_tsk->sp[16] = (uint32_t)func; // PC = task function address
    new_tsk->sp[17] = 1 << 24; // xPSR = 1 << 24 = 0x01000000 , voir page 7 doc 1

    tsk_running = list_insert_tail(tsk_running, new_tsk);
    if(tsk_running == NULL)
    {
    	return -1;
    }
    //list_display(tsk_running);
    return new_tsk->id;
}
/* sys_task_kill
 *   kill oneself
 */
int32_t sys_task_kill()
{
    if (!tsk_running) return -1;

    Task *killed_task;

    tsk_running = list_remove_head(tsk_running, &killed_task);

    free(killed_task);

    if (tsk_running) {
        tsk_running->status = TASK_RUNNING;
        sys_switch_ctx();
    }

    return 0;
}
/* sys_task_id
 *   returns id of task
 */
int32_t sys_task_id()
{
	if(tsk_running != NULL)
		return tsk_running->id;
    return -1;
}

/* sys_task_yield
 *   run scheduler to switch to another task
 */
int32_t sys_task_yield()
{

    return -1;
}

/* task_wait
 *   suspend the current task until timeout
 */
int32_t sys_task_wait(uint32_t ms)
{
    if (!tsk_running) return -1;

    tsk_running->delay = ms;
    tsk_running->status = TASK_SLEEPING;

    Task *sleeping_task;
    tsk_running = list_remove_head(tsk_running, &sleeping_task);

    tsk_sleeping = list_insert_tail(tsk_sleeping, sleeping_task);

    if (tsk_running) {
        tsk_prev = sleeping_task;
        tsk_running->status = TASK_RUNNING;
        sys_switch_ctx();
    }

    return 0;
}

/*****************************************************************************
 * Semaphore handling functions
 *****************************************************************************/

/* sys_sem_new
 *   create a semaphore
 *   init    : initial value
 */
Semaphore * sys_sem_new(int32_t init)
{
    Semaphore *sem = (Semaphore*)malloc(sizeof(Semaphore));
    if (!sem) return NULL;

    sem->count = init;
    sem->waiting = NULL;

    return sem;
}
/* sys_sem_p
 *   take a token
 */
int32_t sys_sem_p(Semaphore * sem)
{
    if (!sem) return -1;

    sem->count--;

    if (sem->count < 0) {
        tsk_running->status = TASK_WAITING;

        Task *waiting_task;
        tsk_running = list_remove_head(tsk_running, &waiting_task);

        sem->waiting = list_insert_tail(sem->waiting, waiting_task);

        if (tsk_running) {
            tsk_prev = waiting_task;
            tsk_running->status = TASK_RUNNING;
            sys_switch_ctx();
        }
    }

    return 0;
}
/* sys_sem_v
 *   release a token
 */
int32_t sys_sem_v(Semaphore * sem)
{
    if (!sem) return -1;

    sem->count++;

    if (sem->count <= 0 && sem->waiting) {
        Task *unblocked_task;
        sem->waiting = list_remove_head(sem->waiting, &unblocked_task);

        unblocked_task->status = TASK_READY;
        tsk_running = list_insert_tail(tsk_running, unblocked_task);

        tsk_prev = tsk_running;
        tsk_running = unblocked_task;
        tsk_running->status = TASK_RUNNING;
        sys_switch_ctx();
    }

    return 0;
}
