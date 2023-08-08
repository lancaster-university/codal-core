/*
The MIT License (MIT)

Copyright (c) 2017 Lancaster University.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "CodalConfig.h"
#include "CodalFiber.h"
#include "Timer.h"
#include "codal_target_hal.h"
#include "CodalDmesg.h"

#define INITIAL_STACK_DEPTH (fiber_initial_stack_base() - 0x04)


/*
 * Statically allocated values used to create and destroy Fibers.
 * required to be defined here to allow persistence during context switches.
 */
namespace codal
{
Fiber *currentFiber = NULL;                        // The context in which the current fiber is executing.
static Fiber *forkedFiber = NULL;                  // The context in which a newly created child fiber is executing.
static Fiber *idleFiber = NULL;                    // the idle task - performs a power efficient sleep, and system maintenance tasks.

/*
 * Scheduler state.
 */
static Fiber *runQueue = NULL;                     // The list of runnable fibers.
static Fiber *sleepQueue = NULL;                   // The list of blocked fibers waiting on a fiber_sleep() operation.
static Fiber *waitQueue = NULL;                    // The list of blocked fibers waiting on an event.
static Fiber *fiberPool = NULL;                    // Pool of unused fibers, just waiting for a job to do.
static Fiber *fiberList = NULL;                    // List of all active Fibers (excludes those in the fiberPool)

/*
 * Scheduler wide flags
 */
static uint8_t fiber_flags = 0;

/*
 * Fibers may perform wait/notify semantics on events. If set, these operations will be permitted on this EventModel.
 */
static EventModel *messageBus = NULL;
}

using namespace codal;

REAL_TIME_FUNC
void codal::queue_fiber(Fiber *f, Fiber **queue)
{
    target_disable_irq();

    // Record which queue this fiber is on.
    f->queue = queue;

    // Add the fiber to the tail of the queue. Although this involves scanning the
    // list, it results in fairer scheduling.
    if (*queue == NULL)
    {
        f->qnext = NULL;
        f->qprev = NULL;
        *queue = f;
    }
    else
    {
        // Scan to the end of the queue.
        // We don't maintain a tail pointer to save RAM (queues are nrmally very short).
        Fiber *last = *queue;

        while (last->qnext != NULL)
            last = last->qnext;

        last->qnext = f;
        f->qprev = last;
        f->qnext = NULL;
    }

    target_enable_irq();
}

REAL_TIME_FUNC
void codal::dequeue_fiber(Fiber *f)
{
    // If this fiber is already dequeued, nothing the there's nothing to do.
    if (f->queue == NULL)
        return;

    // Remove this fiber fromm whichever queue it is on.
    target_disable_irq();

    if (f->qprev != NULL)
        f->qprev->qnext = f->qnext;
    else
        *(f->queue) = f->qnext;

    if(f->qnext)
        f->qnext->qprev = f->qprev;

    f->qnext = NULL;
    f->qprev = NULL;
    f->queue = NULL;

    target_enable_irq();
}

Fiber * codal::get_fiber_list()
{
    return fiberList;
}

REAL_TIME_FUNC
Fiber *getFiberContext()
{
    Fiber *f;

    target_disable_irq();

    if (fiberPool != NULL)
    {
        f = fiberPool;
        dequeue_fiber(f);
    }
    else
    {
        f = new Fiber();

        if (f == NULL) {
            target_enable_irq();
            return NULL;
        }

        f->tcb = tcb_allocate();

        f->stack_bottom = 0;
        f->stack_top = 0;
    }

    target_enable_irq();

    // Ensure this fiber is in suitable state for reuse.
    f->flags = 0;

    #if CONFIG_ENABLED(DEVICE_FIBER_USER_DATA)
    f->user_data = 0;
    #endif

    tcb_configure_stack_base(f->tcb, fiber_initial_stack_base());

    // Add the new Fiber to the list of all fibers
    target_disable_irq();
    f->next = fiberList;
    fiberList = f;
    target_enable_irq();

    return f;
}

void codal::scheduler_init(EventModel &_messageBus)
{
    // If we're already initialised, then nothing to do.
    if (fiber_scheduler_running())
        return;

        // Store a reference to the messageBus provided.
    // This parameter will be NULL if we're being run without a message bus.
    messageBus = &_messageBus;

    // Create a new fiber context
    currentFiber = getFiberContext();

    // Add ourselves to the run queue.
    queue_fiber(currentFiber, &runQueue);

    // Create the IDLE fiber.
    // Configure the fiber to directly enter the idle task.
    idleFiber = getFiberContext();

    tcb_configure_sp(idleFiber->tcb, INITIAL_STACK_DEPTH);
    tcb_configure_lr(idleFiber->tcb, (PROCESSOR_WORD_TYPE)&idle_task);

    if (messageBus)
    {
        // Register to receive events in the NOTIFY channel - this is used to implement wait-notify semantics
        messageBus->listen(DEVICE_ID_NOTIFY, DEVICE_EVT_ANY, scheduler_event, MESSAGE_BUS_LISTENER_IMMEDIATE);
        messageBus->listen(DEVICE_ID_NOTIFY_ONE, DEVICE_EVT_ANY, scheduler_event, MESSAGE_BUS_LISTENER_IMMEDIATE);

        system_timer_event_every_us(SCHEDULER_TICK_PERIOD_US, DEVICE_ID_SCHEDULER, DEVICE_SCHEDULER_EVT_TICK);
        messageBus->listen(DEVICE_ID_SCHEDULER, DEVICE_SCHEDULER_EVT_TICK, scheduler_tick, MESSAGE_BUS_LISTENER_IMMEDIATE);
    }

    fiber_flags |= DEVICE_SCHEDULER_RUNNING;
}

REAL_TIME_FUNC
int codal::fiber_scheduler_running()
{
    if (fiber_flags & DEVICE_SCHEDULER_RUNNING)
        return 1;

    return 0;
}

void codal::scheduler_tick(Event evt)
{
    Fiber *f = sleepQueue;
    Fiber *t;

#if !CONFIG_ENABLED(LIGHTWEIGHT_EVENTS)
    evt.timestamp /= 1000;
#endif

    // Check the sleep queue, and wake up any fibers as necessary.
    while (f != NULL)
    {
        t = f->qnext;

        if (evt.timestamp >= f->context)
        {
            // Wakey wakey!
            dequeue_fiber(f);
            queue_fiber(f,&runQueue);
        }

        f = t;
    }
}

void codal::scheduler_event(Event evt)
{
    Fiber *f = waitQueue;
    Fiber *t;
    int notifyOneComplete = 0;

    // This should never happen.
    // It is however, safe to simply ignore any events provided, as if no messageBus if recorded,
    // no fibers are permitted to block on events.
    if (messageBus == NULL)
        return;

    // Check the wait queue, and wake up any fibers as necessary.
    while (f != NULL)
    {
        t = f->qnext;

        // extract the event data this fiber is blocked on.
        uint16_t id = f->context & 0xFFFF;
        uint16_t value = (f->context & 0xFFFF0000) >> 16;

        // Special case for the NOTIFY_ONE channel...
        if ((evt.source == DEVICE_ID_NOTIFY_ONE && id == DEVICE_ID_NOTIFY) && (value == DEVICE_EVT_ANY || value == evt.value))
        {
            if (!notifyOneComplete)
            {
                // Wakey wakey!
                dequeue_fiber(f);
                queue_fiber(f,&runQueue);
                notifyOneComplete = 1;
            }
        }

        // Normal case.
        else if ((id == DEVICE_ID_ANY || id == evt.source) && (value == DEVICE_EVT_ANY || value == evt.value))
        {
            // Wakey wakey!
            dequeue_fiber(f);
            queue_fiber(f,&runQueue);
        }

        f = t;
    }

    // Unregister this event, as we've woken up all the fibers with this match.
    if (evt.source != DEVICE_ID_NOTIFY && evt.source != DEVICE_ID_NOTIFY_ONE)
        messageBus->ignore(evt.source, evt.value, scheduler_event);
}

static Fiber* handle_fob()
{
    Fiber *f = currentFiber;

    // This is a blocking call, so if we're in a fork on block context,
    // it's time to spawn a new fiber...
    if (f->flags & DEVICE_FIBER_FLAG_FOB)
    {
        // Allocate a TCB from the new fiber. This will come from the tread pool if available,
        // else a new one will be allocated on the heap.

        if (!forkedFiber)
            forkedFiber = getFiberContext();

         // If we're out of memory, there's nothing we can do.
        // keep running in the context of the current thread as a best effort.
        if (forkedFiber != NULL) {
#if CONFIG_ENABLED(DEVICE_FIBER_USER_DATA)
            forkedFiber->user_data = f->user_data;
            f->user_data = NULL;
#endif
            f = forkedFiber;
        }
    }
    return f;
}

void codal::fiber_sleep(unsigned long t)
{
    // If the scheduler is not running, then simply perform a spin wait and exit.
    if (!fiber_scheduler_running())
    {
        target_wait(t);
        return;
    }

    Fiber *f = handle_fob();

    // Calculate and store the time we want to wake up.
    f->context = system_timer_current_time() + t;

    // Remove fiber from the run queue
    dequeue_fiber(f);

    // Add fiber to the sleep queue. We maintain strict ordering here to reduce lookup times.
    queue_fiber(f, &sleepQueue);

    // Finally, enter the scheduler.
    schedule();
}

int codal::fiber_wait_for_event(uint16_t id, uint16_t value)
{
    int ret = fiber_wake_on_event(id, value);

    if(ret == DEVICE_OK)
        schedule();

    return ret;
}

int codal::fiber_wake_on_event(uint16_t id, uint16_t value)
{
    if (messageBus == NULL || !fiber_scheduler_running())
        return DEVICE_NOT_SUPPORTED;

    Fiber *f = handle_fob();

    // Encode the event data in the context field. It's handy having a 32 bit core. :-)
    f->context = (uint32_t)value << 16 | id;

    // Remove ourselves from the run queue
    dequeue_fiber(f);

    // Add ourselves to the sleep queue. We maintain strict ordering here to reduce lookup times.
    queue_fiber(f, &waitQueue);

    // Register to receive this event, so we can wake up the fiber when it happens.
    // Special case for the notify channel, as we always stay registered for that.
    if (id != DEVICE_ID_NOTIFY && id != DEVICE_ID_NOTIFY_ONE)
        messageBus->listen(id, value, scheduler_event, MESSAGE_BUS_LISTENER_IMMEDIATE);

    return DEVICE_OK;
}

#if CONFIG_ENABLED(DEVICE_FIBER_USER_DATA)
#define HAS_THREAD_USER_DATA (currentFiber->user_data != NULL)
#else
#define HAS_THREAD_USER_DATA false
#endif

int codal::invoke(void (*entry_fn)(void))
{
    // Validate our parameters.
    if (entry_fn == NULL)
        return DEVICE_INVALID_PARAMETER;

    if (!fiber_scheduler_running())
        return DEVICE_NOT_SUPPORTED;

    if (currentFiber->flags & (DEVICE_FIBER_FLAG_FOB | DEVICE_FIBER_FLAG_PARENT | DEVICE_FIBER_FLAG_CHILD) || HAS_THREAD_USER_DATA)
    {
        // If we attempt a fork on block whilst already in a fork on block context, or if the thread 
        // already has user data set, simply launch a fiber to deal with the request and we're done.
        create_fiber(entry_fn);
        return DEVICE_OK;
    }

    // Snapshot current context, but also update the Link Register to
    // refer to our calling function.
    save_register_context(currentFiber->tcb);

    // If we're here, there are two possibilities:
    // 1) We're about to attempt to execute the user code
    // 2) We've already tried to execute the code, it blocked, and we've backtracked.

    // If we're returning from the user function and we forked another fiber then cleanup and exit.
    if (currentFiber->flags & DEVICE_FIBER_FLAG_PARENT)
    {
        currentFiber->flags &= ~DEVICE_FIBER_FLAG_FOB;
        currentFiber->flags &= ~DEVICE_FIBER_FLAG_PARENT;
        return DEVICE_OK;
    }

    // Otherwise, we're here for the first time. Enter FORK ON BLOCK mode, and
    // execute the function directly. If the code tries to block, we detect this and
    // spawn a thread to deal with it.
    currentFiber->flags |= DEVICE_FIBER_FLAG_FOB;
    entry_fn();
    #if CONFIG_ENABLED(DEVICE_FIBER_USER_DATA)
    currentFiber->user_data = NULL;
    #endif
    currentFiber->flags &= ~DEVICE_FIBER_FLAG_FOB;

    // If this is is an exiting fiber that for spawned to handle a blocking call, recycle it.
    // The fiber will then re-enter the scheduler, so no need for further cleanup.
    if (currentFiber->flags & DEVICE_FIBER_FLAG_CHILD)
        release_fiber();

     return DEVICE_OK;
}

int codal::invoke(void (*entry_fn)(void *), void *param)
{
    // Validate our parameters.
    if (entry_fn == NULL)
        return DEVICE_INVALID_PARAMETER;

    if (!fiber_scheduler_running())
        return DEVICE_NOT_SUPPORTED;

    if (currentFiber->flags & (DEVICE_FIBER_FLAG_FOB | DEVICE_FIBER_FLAG_PARENT | DEVICE_FIBER_FLAG_CHILD) || HAS_THREAD_USER_DATA)
    {
        // If we attempt a fork on block whilst already in a fork on block context, or if the thread 
        // already has user data set, simply launch a fiber to deal with the request and we're done.
        create_fiber(entry_fn, param);
        return DEVICE_OK;
    }

    // Snapshot current context, but also update the Link Register to
    // refer to our calling function.
    save_register_context(currentFiber->tcb);

    // If we're here, there are two possibilities:
    // 1) We're about to attempt to execute the user code
    // 2) We've already tried to execute the code, it blocked, and we've backtracked.

    // If we're returning from the user function and we forked another fiber then cleanup and exit.
    if (currentFiber->flags & DEVICE_FIBER_FLAG_PARENT)
    {
        currentFiber->flags &= ~DEVICE_FIBER_FLAG_FOB;
        currentFiber->flags &= ~DEVICE_FIBER_FLAG_PARENT;
        return DEVICE_OK;
    }

    // Otherwise, we're here for the first time. Enter FORK ON BLOCK mode, and
    // execute the function directly. If the code tries to block, we detect this and
    // spawn a thread to deal with it.
    currentFiber->flags |= DEVICE_FIBER_FLAG_FOB;
    entry_fn(param);
    #if CONFIG_ENABLED(DEVICE_FIBER_USER_DATA)
    currentFiber->user_data = NULL;
    #endif
    currentFiber->flags &= ~DEVICE_FIBER_FLAG_FOB;

    // If this is is an exiting fiber that for spawned to handle a blocking call, recycle it.
    // The fiber will then re-enter the scheduler, so no need for further cleanup.
    if (currentFiber->flags & DEVICE_FIBER_FLAG_CHILD)
        release_fiber(param);

    return DEVICE_OK;
}

void codal::launch_new_fiber(void (*ep)(void), void (*cp)(void))
{
    // Execute the thread's entrypoint
    ep();

    // Execute the thread's completion routine;
    cp();

    // If we get here, then the completion routine didn't recycle the fiber... so do it anyway. :-)
    release_fiber();
}

void codal::launch_new_fiber_param(void (*ep)(void *), void (*cp)(void *), void *pm)
{
    // Execute the thread's entrypoint.
    ep(pm);

    // Execute the thread's completion routine.
    cp(pm);

    // If we get here, then the completion routine didn't recycle the fiber... so do it anyway. :-)
    release_fiber(pm);
}


Fiber *__create_fiber(uint32_t ep, uint32_t cp, uint32_t pm, int parameterised)
{
    // Validate our parameters.
    if (ep == 0 || cp == 0)
        return NULL;

    // Allocate a TCB from the new fiber. This will come from the fiber pool if available,
    // else a new one will be allocated on the heap.
    Fiber *newFiber = getFiberContext();

    // If we're out of memory, there's nothing we can do.
    if (newFiber == NULL)
        return NULL;

    tcb_configure_args(newFiber->tcb, ep, cp, pm);
    tcb_configure_sp(newFiber->tcb, INITIAL_STACK_DEPTH);
    tcb_configure_lr(newFiber->tcb, parameterised ? (PROCESSOR_WORD_TYPE) &launch_new_fiber_param : (PROCESSOR_WORD_TYPE) &launch_new_fiber);

    // Add new fiber to the run queue.
    queue_fiber(newFiber, &runQueue);

    return newFiber;
}

Fiber *codal::create_fiber(void (*entry_fn)(void), void (*completion_fn)(void))
{
    if (!fiber_scheduler_running())
        return NULL;

    return __create_fiber((uint32_t) entry_fn, (uint32_t)completion_fn, 0, 0);
}

Fiber *codal::create_fiber(void (*entry_fn)(void *), void *param, void (*completion_fn)(void *))
{
    if (!fiber_scheduler_running())
        return NULL;

    return __create_fiber((uint32_t) entry_fn, (uint32_t)completion_fn, (uint32_t) param, 1);
}

void codal::release_fiber(void *)
{
    if (!fiber_scheduler_running())
        return;

    release_fiber();
}

REAL_TIME_FUNC
void codal::release_fiber(void)
{
    if (!fiber_scheduler_running())
        return;

    // Remove ourselves form the runqueue.
    dequeue_fiber(currentFiber);

    // Add ourselves to the list of free fibers
    queue_fiber(currentFiber, &fiberPool);

    // limit the number of fibers in the pool
    int numFree = 0;
    for (Fiber *p = fiberPool; p; p = p->qnext) {
        if (!p->qnext && numFree > 3) {
            p->qprev->qnext = NULL;
            free(p->tcb);
            free((void *)p->stack_bottom);
            memset(p, 0, sizeof(*p));
            free(p);
            break;
        }
        numFree++;
    }

    // Reset fiber state, to ensure it can be safely reused.
    currentFiber->flags = 0;
    tcb_configure_stack_base(currentFiber->tcb, fiber_initial_stack_base());

    // Remove the fiber from the list of active fibers
    target_disable_irq();
    if (fiberList == currentFiber)
    {
        fiberList = fiberList->next;
    }
    else
    {
        Fiber *p = fiberList;

        while (p)
        {
            if (p->next == currentFiber)
            {
                p->next = currentFiber->next;
                break;
            }

            p = p->next;
        }
    }
    target_enable_irq();

    // Find something else to do!
    schedule();
}

void codal::verify_stack_size(Fiber *f)
{
    // Ensure the stack buffer is large enough to hold the stack Reallocate if necessary.
    PROCESSOR_WORD_TYPE stackDepth;
    PROCESSOR_WORD_TYPE bufferSize;

    // Calculate the stack depth.
    stackDepth = tcb_get_stack_base(f->tcb) - (PROCESSOR_WORD_TYPE)get_current_sp();

    // Calculate the size of our allocated stack buffer
    bufferSize = f->stack_top - f->stack_bottom;

    // If we're too small, increase our buffer size.
    if (bufferSize < stackDepth)
    {
        // We are only here, when the current stack is the stack of fiber [f].
        // Make sure the contents of [currentFiber] variable reflects that, otherwise
        // an external memory allocator might get confused when scanning fiber stacks.
        Fiber *prevCurrFiber = currentFiber;
        currentFiber = f;

        // GCC would normally assume malloc() and free() can't access currentFiber variable
        // and thus skip emitting the store above.
        // We invoke an external function that GCC knows nothing about (any function will do)
        // to force GCC to emit the store.
        get_current_sp();

        // To ease heap churn, we choose the next largest multple of 32 bytes.
        bufferSize = (stackDepth + 32) & 0xffffffe0;

        // Release the old memory
        if (f->stack_bottom != 0)
            free((void *)f->stack_bottom);

        // Allocate a new one of the appropriate size.
        f->stack_bottom = (PROCESSOR_WORD_TYPE)malloc(bufferSize);

        // Recalculate where the top of the stack is and we're done.
        f->stack_top = f->stack_bottom + bufferSize;

        currentFiber = prevCurrFiber;
    }
}

int codal::scheduler_runqueue_empty()
{
    return (runQueue == NULL);
}

int codal::scheduler_waitqueue_empty()
{
    return (waitQueue == NULL);
}

void codal::schedule()
{
    if (!fiber_scheduler_running())
        return;

    // First, take a reference to the currently running fiber;
    Fiber *oldFiber = currentFiber;

    // First, see if we're in Fork on Block context. If so, we simply want to store the full context
    // of the currently running thread in a newly created fiber, and restore the context of the
    // currently running fiber, back to the point where it entered FOB.

    if (currentFiber->flags & DEVICE_FIBER_FLAG_FOB)
    {
        // Record that the fibers have a parent/child relationship
        currentFiber->flags |= DEVICE_FIBER_FLAG_PARENT;
        forkedFiber->flags |= DEVICE_FIBER_FLAG_CHILD;

        // Define the stack base of the forked fiber to be align with the entry point of the parent fiber
        tcb_configure_stack_base(forkedFiber->tcb, tcb_get_sp(currentFiber->tcb));

        // Ensure the stack allocation of the new fiber is large enough
        verify_stack_size(forkedFiber);

        // Store the full context of this fiber.
        save_context(forkedFiber->tcb, forkedFiber->stack_top);

        // Indicate that we have completed spawning a new fiber
        forkedFiber = NULL;

        // We may now be either the newly created thread, or the one that created it.
        // if the DEVICE_FIBER_FLAG_PARENT flag is still set, we're the old thread, so
        // restore the current fiber to its stored context and we're done.
        if (currentFiber->flags & DEVICE_FIBER_FLAG_PARENT)
            restore_register_context(currentFiber->tcb);

        // If we're the new thread, we must have been unblocked by the scheduler, so simply return
        // and continue processing.
        return;
    }

    // We're in a normal scheduling context, so perform a round robin algorithm across runnable fibers.
    // OK - if we've nothing to do, then run the IDLE task (power saving sleep)
    if (runQueue == NULL)
        currentFiber = idleFiber;

    else if (currentFiber->queue == &runQueue)
        // If the current fiber is on the run queue, round robin.
        currentFiber = currentFiber->qnext == NULL ? runQueue : currentFiber->qnext;

    else
        // Otherwise, just pick the head of the run queue.
        currentFiber = runQueue;

    if (currentFiber == idleFiber && oldFiber->flags & DEVICE_FIBER_FLAG_DO_NOT_PAGE)
    {
        // Run the idle task right here using the old fiber's stack.
        // Keep idling while the runqueue is empty, or there is data to process.

        // Run in the context of the original fiber, to preserve state of flags...
        // as we are running on top of this fiber's stack.
        currentFiber = oldFiber;

        do
        {
            idle();
        }
        while (runQueue == NULL);

        // Switch to a non-idle fiber.
        // If this fiber is the same as the old one then there'll be no switching at all.
        currentFiber = runQueue;
    }

    // Swap to the context of the chosen fiber, and we're done.
    // Don't bother with the overhead of switching if there's only one fiber on the runqueue!
    if (currentFiber != oldFiber)
    {

        // Special case for the idle task, as we don't maintain a stack context (just to save memory).
        if (currentFiber == idleFiber)
        {
            tcb_configure_sp(idleFiber->tcb, INITIAL_STACK_DEPTH);
            tcb_configure_lr(idleFiber->tcb, (PROCESSOR_WORD_TYPE)&idle_task);
        }

        // If we're returning for IDLE or our last fiber has been destroyed, we don't need to waste time
        // saving the processor context - Just swap in the new fiber, and discard changes to stack and register context.
        if (oldFiber == idleFiber || oldFiber->queue == &fiberPool)
        {
            swap_context(NULL, 0, currentFiber->tcb, currentFiber->stack_top);
        }
        else
        {
            // Ensure the stack allocation of the fiber being scheduled out is large enough
            verify_stack_size(oldFiber);

            // Schedule in the new fiber.
            swap_context(oldFiber->tcb, oldFiber->stack_top, currentFiber->tcb, currentFiber->stack_top);
        }
    }
}

void codal::idle()
{
    // Prevent an idle loop of death:
    // We will return to idle after processing any idle events that add anything
    // to our run queue, we use the DEVICE_SCHEDULER_IDLE flag to determine this
    // scenario.
    if(!(fiber_flags & DEVICE_SCHEDULER_IDLE))
    {
        fiber_flags |= DEVICE_SCHEDULER_IDLE;
        Event(DEVICE_ID_SCHEDULER, DEVICE_SCHEDULER_EVT_IDLE);
    }

    // If the above did create any useful work, enter power efficient sleep.
    if(scheduler_runqueue_empty())
    {
        // unset our DEVICE_SCHEDULER_IDLE flag, we have processed all of the events
        // because we enforce MESSAGE_BUS_LISTENER_IMMEDIATE for listeners placed
        // on the scheduler.
        fiber_flags &= ~DEVICE_SCHEDULER_IDLE;
        target_scheduler_idle();
    }
}

void codal::idle_task()
{
    while(1)
    {
        idle();
        schedule();
    }
}

int codal::fiber_scheduler_get_deepsleep_pending()
{
    return fiber_flags & DEVICE_SCHEDULER_DEEPSLEEP ? 1 : 0;
}

void codal::fiber_scheduler_set_deepsleep_pending( int pending)
{
    if ( pending)
        fiber_flags |= DEVICE_SCHEDULER_DEEPSLEEP;
    else
        fiber_flags &= ~DEVICE_SCHEDULER_DEEPSLEEP;
}

FiberLock::FiberLock( int initial, FiberLockMode mode )
{
    this->queue = NULL;
    this->locked = initial;
    this->resetTo = initial;
    this->mode = mode;
}


REAL_TIME_FUNC
void FiberLock::wait()
{
    // If the scheduler is not running, then simply exit, as we're running monothreaded.
    if (!fiber_scheduler_running())
        return;

    target_disable_irq();
    int l = --locked;
    target_enable_irq();

    //DMESGF( "%d, wait(%d)", (uint32_t)this & 0xFFFF, locked );

    if (l < 0)
    {
        // wait() is a blocking call, so if we're in a fork on block context,
        // it's time to spawn a new fiber...
        Fiber *f = handle_fob();

        // Remove fiber from the run queue
        dequeue_fiber(f);

        // Add fiber to the sleep queue. We maintain strict ordering here to reduce lookup times.
        queue_fiber(f, &queue);

        // Check if we've been raced by something running in interrupt context.
        // Note this is safe, as no IRQ can wait() and as we are non-preemptive, neither could any other fiber.
        // It is possible that and IRQ has performed a notify() operation however.
        // If so, put ourself back on the run queue and spin the scheduler (in case we performed a fork-on-block)
        target_disable_irq();
        if (locked < l)
        {
            // Remove fiber from the run queue
            dequeue_fiber(f);

            // Add fiber to the sleep queue. We maintain strict ordering here to reduce lookup times.
            queue_fiber(f, &runQueue);
        }
        target_enable_irq();

        // Finally, enter the scheduler.
        schedule();
    }
}

void FiberLock::notify()
{
    locked++;
    //DMESGF( "%d, notify(%d)", (uint32_t)this & 0xFFFF, locked );
    Fiber *f = queue;
    if (f)
    {
        dequeue_fiber(f);
        queue_fiber(f, &runQueue);
    }
}

void FiberLock::notifyAll()
{
    //DMESGF( "%d, notifyAll(%d)", (uint32_t)this & 0xFFFF, locked );
    Fiber *f = queue;
    while (f)
    {
        this->notify();
        f = queue;
    }

    if( this->mode == FiberLockMode::MUTEX )
        this->locked = this->resetTo;

    //DMESGF( "%d, { notifyAll(%d) }", (uint32_t)this & 0xFFFF, locked );
}

int FiberLock::getWaitCount()
{
    if( locked > -1 )
        return 0;
    return 0 - locked;
}
