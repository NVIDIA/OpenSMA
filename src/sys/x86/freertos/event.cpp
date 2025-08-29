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
#include "event.h"

#include <cerrno>
#include <chrono>
#include <ctime>
#include <pthread.h>

using namespace sys::freertos;

Event::Event() : _triggered(false)
{
    pthread_mutex_init(&_mutex, nullptr);
    pthread_cond_init(&_cond, nullptr);
}

// GBS:BEGIN NO COVERAGE FIXME!!
Event::~Event()
{
    pthread_mutex_destroy(&_mutex);
    pthread_cond_destroy(&_cond);
}

bool Event::wait_timed(std::chrono::milliseconds ms)
{
    using namespace std::chrono;

    timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    auto secs   = duration_cast<seconds>(ms);
    ts.tv_sec  += secs.count();
    ts.tv_nsec += duration_cast<nanoseconds>(ms - secs).count();

    pthread_mutex_lock(&_mutex);
    while (!_triggered) {
        switch (pthread_cond_timedwait(&_cond, &_mutex, &ts)) {
            case ETIMEDOUT: [[fallthrough]];
            case EINVAL   : [[fallthrough]];
            case EPERM    : return false;
            default       : break;
        }
    }
    _triggered = false;
    pthread_mutex_unlock(&_mutex);
    return true;
}
// GBS:END NO COVERAGE FIXME!!

bool Event::wait()
{
    pthread_mutex_lock(&_mutex);

    while (_triggered == false) {
        pthread_cond_wait(&_cond, &_mutex);
    }

    _triggered = false;
    pthread_mutex_unlock(&_mutex);
    return true;
}

void Event::signal()
{
    pthread_mutex_lock(&_mutex);
    _triggered = true;
    pthread_cond_signal(&_cond);
    pthread_mutex_unlock(&_mutex);
}
