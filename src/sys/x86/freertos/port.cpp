/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <algorithm>
#include <bit>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <FreeRTOS.h>
#include <portmacro.h>
#include <pthread.h>
#include <task.h>
#include <timers.h>
#include <sys/time.h>
#include <sys/times.h>

#include "FreeRTOSConfig.h"

#include "freertos/event.h"
#include "nv/common/utils.h"
#include "nv/nv.h"

// NOLINTBEGIN(misc-include-cleaner)  pthread issues

namespace {

/// pthread implementation of a FreeRTOS task.
class Thread
{
    using Event = sys::freertos::Event;

    struct State
    {
        int       critical_nesting{};
        pthread_t main_thread{};
    };

public:
    static auto& state()
    {
        static auto&& state = State();  // NOLINT
        return state;
    }

    /// Set the parent thread that all other threads inherit from.
    static void set_main_thread()
    {
        state().main_thread = pthread_self();
        pthread_setname_np(pthread_self(), "Scheduler");
    }

    /// Kill the main thread.
    static void kill_main()
    {
        // NOLINTNEXTLINE(misc-include-cleaner)
        pthread_kill(Thread::state().main_thread, SIGUSR1);
    }

    /// Get the Currently active thread.
    static Thread& current() { return from_task(xTaskGetCurrentTaskHandle()); }

    /// Get a Thread object from a task handle.
    static Thread& from_task(TaskHandle_t task)
    {
        auto stack = *std::bit_cast<StackType_t**>(task);
        return *std::bit_cast<Thread*>(stack + 1);
    }

    /// pthread's main entrypoint
    static void* thread_start(void* params)
    {
        pthread_setname_np(pthread_self(), "unnamed");
        // start suspended
        auto thread = static_cast<Thread*>(params);
        thread->suspend();

        // set thread name
        auto name = pcTaskGetName(xTaskGetCurrentTaskHandle());
        pthread_setname_np(pthread_self(), name);

        state().critical_nesting = 0;
        vPortEnableInterrupts();

        // call task's entrypoint
        nv::debug("Starting thread '%s:%x'...\n", name, thread->pt_handle());
        thread->thread_main(thread->params);

        // thread is exiting... shouldn't happen except from UNIT
        if (name[0] != 'U' || name[1] != 'N' || name[2] != 'I' || name[3] != 'T') {
            // GBS:BEGIN NO COVERAGE FIXME!!
            nv::fatal("thread is exiting without vTaskDelete(nullptr) '%s'...\n", name);
            // GBS:END NO COVERAGE FIXME!!
        }

        return nullptr;
    };

    /// Creates a new pthread for a task.
    Thread(pdTASK_CODE     main,
           void*           params,
           portSTACK_TYPE* stack_top,
           portSTACK_TYPE* stack_bottom)
    : thread_main(main)
    , params(params)
    {
        auto stack_size = std::max(size_t(PTHREAD_STACK_MIN),
                                   (stack_top + 1 - stack_bottom) * sizeof(*stack_top));

        pthread_attr_t attrs{};  // NOLINT(misc-include-cleaner)
        pthread_attr_init(&attrs);
        if (pthread_attr_setstack(&attrs, stack_bottom, stack_size) != 0) {
            nv::fatal("pthread_attr_setstack\n");  // GBS: NO COVERAGE FIXME!!
        }
        vPortEnterCritical();

        // start the thread in a suspended state
        if (pthread_create(&handle, &attrs, thread_start, this) != 0) {
            nv::fatal("pthread_create\n");  // GBS: NO COVERAGE FIXME!!
        }

        vPortExitCritical();
    }

    ~Thread() = default;
    NV_COMMON_COPY_MOVE(Thread, delete);

    /// Changing thread context.
    void switch_to(Thread& other)
    {
        if (this != &other) {
            auto save_critical_nesting = state().critical_nesting;
            other.resume();
            if (is_dying) {
                pthread_exit(nullptr);
            }
            suspend();
            state().critical_nesting = save_critical_nesting;
        }
    }

    /// signal the thread's event and continue on from a suspend.
    void resume()
    {
        if (pthread_self() != handle) {
            event.signal();
        }
    }

    /// Suspend this thread.
    void suspend()
    {
        event.wait();
        pthread_testcancel();
    }

    /// Thread cleanup
    void cancel()
    {
        pthread_cancel(handle);
        event.signal();
        pthread_join(handle, nullptr);
    }

    /// Thread is dying.
    void dying() { is_dying = true; }

    const pthread_t& pt_handle() const { return handle; }

private:
    pthread_t   handle{};
    pdTASK_CODE thread_main{};
    void*       params{};
    bool        is_dying = false;
    Event       event;
};

class Scheduler
{
public:
    static auto& inst()
    {
        static auto&& sched = Scheduler();  // NOLINT
        return sched;
    }

    /// Start the Scheduler
    int start()
    {
        Thread::set_main_thread();
        inst().running = true;
        setup_interrupt();

        sigset_t sigs;
        sigemptyset(&sigs);
        sigaddset(&sigs, SIGUSR1);
        pthread_sigmask(SIG_BLOCK, &sigs, nullptr);

        Thread::current().resume();  // start first task

        while (inst().running) {
            int sig{};
            sigwait(&sigs, &sig);
        }

        pthread_sigmask(SIG_SETMASK, &inst().original_sigmask, nullptr);
        vPortCancelThread(xTimerGetTimerDaemonTaskHandle());
        vPortCancelThread(xTaskGetIdleTaskHandle());

        return 0;
    }

    /// Stop the thread and clean up.
    void stop()
    {
        itimerval itimer{};
        itimer.it_value.tv_sec     = 0;
        itimer.it_value.tv_usec    = 0;
        itimer.it_interval.tv_sec  = 0;
        itimer.it_interval.tv_usec = 0;
        if (setitimer(ITIMER_REAL, &itimer, nullptr) < 0) {
            nv::common::fatal("setitimer\n");  // GBS: NO COVERAGE FIXME!!
        }

        struct sigaction sigtick
        {};
        sigtick.sa_flags   = 0;
        sigtick.sa_handler = SIG_IGN;
        sigemptyset(&sigtick.sa_mask);
        sigaction(SIGALRM, &sigtick, nullptr);

        inst().running = false;
        pthread_join(interrupt_thread, nullptr);
        Thread::kill_main();
    }

    void disable_interrupts() { pthread_sigmask(SIG_BLOCK, &inst().all_signals, nullptr); }
    void enable_interrupts() { pthread_sigmask(SIG_UNBLOCK, &inst().all_signals, nullptr); }

    /// Blocks all signals other than SIGINT and installs a tick handler for SIGALRM
    void setup_signals()
    {
        if (initialized == false) {
            initialized = true;
            sigfillset(&all_signals);         // enabled for all signals
            sigdelset(&all_signals, SIGINT);  // disabled for SIGINT

            // block the signals for the calling thread
            pthread_sigmask(SIG_SETMASK, &inst().all_signals, &inst().original_sigmask);

            // install the tick handler on SIGALRM
            struct sigaction sigtick
            {};
            sigtick.sa_flags   = 0;
            sigtick.sa_handler = tick_handler;
            sigfillset(&sigtick.sa_mask);

            if (sigaction(SIGALRM, &sigtick, nullptr) < 0) {
                nv::common::fatal("sigaction %x\n", errno);  // GBS: NO COVERAGE FIXME!!
            }
        }
    }

private:
    /// Set up the interrupt timer to generate SIGALRM every portTICK_RATE_MICROSECONDS
    void setup_interrupt()
    {
        pthread_create(
            &interrupt_thread,
            nullptr,
            [](void* args) -> void* {
                constexpr auto OneSecInUsecs = 1'000'000;
                pthread_setname_np(pthread_self(), "interrupt_thread");
                auto& is_alive = *std::bit_cast<bool*>(args);
                while (is_alive) {
                    auto& t = Thread::current();
                    pthread_kill(t.pt_handle(), SIGALRM);
                    usleep(OneSecInUsecs / configTICK_RATE_HZ);
                }
                return nullptr;
            },
            &running);
    }

    static void tick_handler(int sig)
    {
        if (sig == SIGALRM) {
            Thread::state().critical_nesting++;
            auto& to_suspend = Thread::current();
            xTaskIncrementTick();
            vTaskSwitchContext();
            to_suspend.switch_to(Thread::current());
            Thread::state().critical_nesting--;
        }
    }

    bool      initialized = false;
    bool      running     = false;
    sigset_t  all_signals{};
    sigset_t  original_sigmask{};
    pthread_t interrupt_thread{};
};

}  // namespace

extern "C" {

// Call by FreeRTOS when we call task.setup()
portSTACK_TYPE* pxPortInitialiseStack(portSTACK_TYPE* stack_top,
                                      portSTACK_TYPE* stack_bottom,
                                      pdTASK_CODE     thread_main,
                                      void*           params)

{
    Scheduler::inst().setup_signals();

    // store the thread data at the stop of the stack
    auto mem  = std::bit_cast<Thread*>(stack_top + 1) - 1;
    stack_top = std::bit_cast<portSTACK_TYPE*>(mem) - 1;

    // create the main thread
    // NOLINTNEXTLINE
    [[maybe_unused]] auto thread = new (mem)
        Thread(thread_main, params, stack_top, stack_bottom);

    return stack_top;
}

// Connect up to freertos
portBASE_TYPE xPortStartScheduler()
{
    return Scheduler::inst().start();
}
void vPortEndScheduler()
{
    Scheduler::inst().stop();
}
void vPortDisableInterrupts()
{
    Scheduler::inst().disable_interrupts();
}
void vPortEnableInterrupts()
{
    Scheduler::inst().enable_interrupts();
}

void vPortEnterCritical()
{
    if (Thread::state().critical_nesting == 0) {
        vPortDisableInterrupts();
    }
    Thread::state().critical_nesting++;
}

void vPortExitCritical()
{
    Thread::state().critical_nesting--;
    if (Thread::state().critical_nesting == 0) {
        vPortEnableInterrupts();
    }
}

void vPortYield()
{
    vPortEnterCritical();

    auto& thread_to_suspend = Thread::current();
    vTaskSwitchContext();
    thread_to_suspend.switch_to(Thread::current());

    vPortExitCritical();
}

void vPortThreadDying(void* task_to_delete, [[maybe_unused]] volatile BaseType_t* pending_yield)
{
    Thread::from_task(static_cast<TaskHandle_t>(task_to_delete)).dying();
}

void vPortCancelThread(void* task_to_delete)
{
    auto& pxThreadToCancel = Thread::from_task(static_cast<TaskHandle_t>(task_to_delete));
    pxThreadToCancel.cancel();
}

// GBS:BEGIN NO COVERAGE FIXME!!
void vPortStoreTaskMPUSettings([[maybe_unused]] xMPU_SETTINGS*    mpu_settings,
                               const struct xMEMORY_REGION* const regions,
                               [[maybe_unused]] StackType_t*      bottom_of_stack,
                               [[maybe_unused]] uint32_t          stack_depth)
{
    // TODO: implement a basic MPU using pkey
    if (regions) {
        nv::warn("MPU emulation not implemented...\n");
    }
}
unsigned long ulPortGetRunTime()  // NOLINT
{
    tms tms{};
    times(&tms);
    return tms.tms_utime;
}
portBASE_TYPE xPortSetInterruptMask()
{
    return pdTRUE;
}
void vPortClearInterruptMask(portBASE_TYPE xMask) {}
// GBS:END NO COVERAGE FIXME!!
}

// NOLINTEND(misc-include-cleaner)
