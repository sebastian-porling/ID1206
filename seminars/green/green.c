#include <stdlib.h>
#include <stdio.h>
#include <ucontext.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>
#include "green.h"

#define FALSE 0
#define TRUE 1

#define STACK_SIZE 4096

#define PERIOD 100

static sigset_t block;

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, FALSE};

static green_t *running = &main_green;

// ready queue
typedef struct queue{
    green_t *first;
    green_t *last;
} queue;

queue ready_queue = {0};

void timer_handler(int);

static void init() __attribute__((constructor));

void init(){
    sigemptyset(&block);
    sigaddset(&block, SIGVTALRM);

    struct sigaction act = {0};
    struct timeval interval;
    struct itimerval period;

    act.sa_handler = timer_handler;
    assert(sigaction(SIGVTALRM, &act, NULL) == 0);

    interval.tv_sec = 0;
    interval.tv_usec = PERIOD;
    period.it_interval = interval;
    period.it_value = interval;
    setitimer(ITIMER_VIRTUAL, &period, NULL);

    getcontext(&main_cntx);
}

int green_mutex_init(green_mutex_t *mutex){
    mutex->taken = FALSE;
    mutex->susp = NULL;
}

void green_cond_init(green_cond_t *cond){
     cond->susp = NULL;
}

green_t *pop(){
    green_t *next = ready_queue.first;
    if(next != NULL){
        ready_queue.first = ready_queue.first->next;
        next->next = NULL;
    }
    if(ready_queue.first == NULL){
        ready_queue.last == NULL;
    }
    if(next == NULL){
        printf("Popppppp\n");
        while(ready_queue.first == NULL){
            
        };
        //next = &main_green;
    }
    return next;
}

void *push(green_t *thread){
    if(thread != NULL){
        if(ready_queue.first == NULL){
            ready_queue.first = thread;
            ready_queue.last = thread;
        }else if(ready_queue.last != NULL){
            ready_queue.last->next = thread;
            ready_queue.last = thread;
            if(thread->next != NULL){
                push(thread->next);
            }
        }
    }
}

void *push_mutex(green_mutex_t *mutex){
    if(mutex->susp != NULL){
        if(ready_queue.first == NULL){
            ready_queue.first = mutex->susp;
            ready_queue.last = mutex->susp;
            mutex->susp = NULL;
        }else if(ready_queue.last != NULL){
            green_t *thread = mutex->susp;
            ready_queue.last->next = thread;
            ready_queue.last = thread;
            mutex->susp = mutex->susp->next;
        }
    }
}

void addNext(green_t *this, green_t *thread){
    if(this->next != NULL){
        addNext(this->next, thread);
    }else{
        this->next = thread;
    }
}

int green_mutex_lock(green_mutex_t *mutex){
    // block timer interrupt
    sigprocmask(SIG_BLOCK, &block, NULL);

    green_t *susp = running;
    
    while(mutex->taken){
        // suspend the running thread
        if(mutex->susp == NULL){
            mutex->susp = susp;
        }else{
            addNext(mutex->susp, susp);
        }
        // find the next thread
        green_t *next = pop();
        running = next;
        swapcontext(susp->context, next->context);
    }
    // take the lock
    mutex->taken = TRUE;

    //unblock
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}

int green_mutex_unlock(green_mutex_t *mutex){
    // block timer interrupt
    sigprocmask(SIG_BLOCK, &block, NULL);

    // move suspended threads to ready queue
    push(mutex->susp);
    // release lock
    mutex->taken = FALSE;
    mutex->susp = NULL;
    // unblock
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}

void timer_handler(int sig){
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *susp = running;
    printf("HEJHEJ\n");
    // add the running to the ready queue
    push(running);
    // find the next thread for execution
    green_t *next = pop();
    running = next;
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    swapcontext(susp->context, next->context);
}

void green_thread(){
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *this = running;

    (*this->fun)(this->arg);

    // place waiting (joining) thread in ready queue
    push(this->join);
    this->join = NULL;
    // free alocated memory strucures
    free(this->context->uc_stack.ss_sp);
    this->context->uc_stack.ss_size = 0;
    free(this->context);
    // we're a zombie
    this->zombie = 1;
    //find the next thread to run
    green_t *next = pop();
    running = next;
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    setcontext(next->context);
}

int green_yield(){
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *susp = running;

    //add susp to ready queue
    push(susp);

    // select the next thread for execution
    green_t *next = pop();

    running = next;
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    swapcontext(susp->context, next->context);
    return 0;
}



int green_cond_wait(green_cond_t* cond, green_mutex_t *mutex){
    sigprocmask(SIG_BLOCK, &block, NULL);
    green_t *susp = running;
    if(cond->susp != NULL){
        addNext(cond->susp, susp);
    }else{
        cond->susp = susp; 
    }

    if(mutex != NULL){
        // release the lock of we have a mutex
        mutex->taken = FALSE;
        // schedule suspended threads
        push(mutex->susp);
        mutex->susp = NULL;
    }
    // schedule the next thread
    green_t *next = pop();

    if(next != NULL){
        running = next;
        swapcontext(susp->context, next->context);
    }

    if(mutex != NULL){
        // try to take the lock
        while(mutex->taken){
            // bad luck, suspend
            susp = running;
            next = pop();
            running = next;
            swapcontext(susp->context, next->context);
        }
        // take the lock
        mutex->taken == TRUE;
    }
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}

void *push_cond(green_cond_t *cond){
    if(cond->susp != NULL){
        if(ready_queue.first == NULL){
            ready_queue.first = cond->susp;
            ready_queue.last = cond->susp;
            cond->susp = NULL;
        }else if(ready_queue.last != NULL){
            green_t *thread = cond->susp;
            ready_queue.last->next = thread;
            ready_queue.last = thread;
            ready_queue.last->next = NULL;
            cond->susp = cond->susp->next;
        }
    }
}

void green_cond_signal(green_cond_t *cond){
    sigprocmask(SIG_BLOCK, &block, NULL);
    if(cond->susp != NULL){
        push_cond(cond);
    }
    sigprocmask(SIG_UNBLOCK, &block, NULL);
}

int green_join(green_t *thread){
    sigprocmask(SIG_BLOCK, &block, NULL);
    if (thread->zombie) {
        sigprocmask(SIG_UNBLOCK, &block, NULL);
        return 0;
    }
    green_t *susp = running;
    // add to waiting threads
    if(thread->join == NULL){
        thread->join = susp;
    }else{
        addNext(thread->join, susp);
    }
    // select the next thread for execution
    green_t *next = pop();
    if(next != NULL){
        running = next;
        sigprocmask(SIG_UNBLOCK, &block, NULL);
        swapcontext(susp->context, next->context);
    }
    return 0;
}

int green_create(green_t *new, void *(*fun)(void*), void *arg){
    sigprocmask(SIG_BLOCK, &block, NULL);
    ucontext_t *cntx = (ucontext_t *)malloc(sizeof(ucontext_t));
    getcontext(cntx);
    void *stack = malloc(STACK_SIZE);

    cntx->uc_stack.ss_sp = stack;
    cntx->uc_stack.ss_size = STACK_SIZE;
    makecontext(cntx, green_thread, 0);
    new->context = cntx;
    new->fun = fun;
    new->arg = arg;
    new->next = NULL;
    new->join = NULL;
    new->zombie = FALSE;
    // Add new to the ready queue
    push(new);
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}
