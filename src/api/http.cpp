/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/http.h"

namespace http {

namespace {
size_t s_worker_stack = 0;   /* 0: kWorkerStack */
void (*s_thread_start)() = nullptr;
}

void set_worker_stack(size_t bytes) { s_worker_stack = bytes; }
void set_thread_start(void (*hook)()) { s_thread_start = hook; }

void *Pool::start(void *pool)
{
    if (s_thread_start)
        s_thread_start();
    static_cast<Pool *>(pool)->run();
    return nullptr;
}

Pool::Pool(Transport &transport, int threads, std::function<void()> wake)
    : m_transport(transport), m_wake(std::move(wake))
{
    for (int i = 0; i < threads; i++) {
        pthread_attr_t attr;
        const bool has_attr = pthread_attr_init(&attr) == 0;
        if (has_attr) {
            size_t size = 0;
            pthread_attr_getstacksize(&attr, &size);
            const size_t want = s_worker_stack ? s_worker_stack : kWorkerStack;
            if (s_worker_stack || size < want)
                pthread_attr_setstacksize(&attr, want);
        }
        pthread_t t;
        if (pthread_create(&t, has_attr ? &attr : nullptr, &Pool::start, this) == 0 ||
            pthread_create(&t, nullptr, &Pool::start, this) == 0)
            m_threads.push_back(t);
        if (has_attr)
            pthread_attr_destroy(&attr);
    }
}

Pool::~Pool()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop = true;
        m_jobs.clear();
    }
    m_cv.notify_all();
    for (pthread_t t : m_threads)
        pthread_join(t, nullptr);
}

void Pool::submit(Request req, Work work)
{
    m_busy++;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_jobs.push_back({std::move(req), std::move(work)});
    }
    m_cv.notify_one();
}

int Pool::pump()
{
    std::vector<std::function<void()>> done;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        done.swap(m_done);
    }
    for (auto &f : done)
        if (f)
            f();
    return (int)done.size();
}

void Pool::run()
{
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_stop || !m_jobs.empty(); });
            if (m_stop)
                return;
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
        }
        Response resp = m_transport.perform(job.req, &m_stop);
        std::function<void()> then = job.work ? job.work(resp) : nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_done.push_back(std::move(then));
        }
        m_busy--;
        if (m_wake)
            m_wake();
    }
}

std::string header(const std::string &name, const std::string &value) { return name + ": " + value; }

} // namespace http
