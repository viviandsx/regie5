/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "api/http.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace http {

namespace {
size_t s_worker_stack = 0;   /* 0: kWorkerStack */
void (*s_thread_start)() = nullptr;
}

/* Seconds on a clock that never steps (the PS5's wall clock is corrected at start). */
double clock_now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
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

std::string Pool::server_of(const Request &req)
{
    if (!req.server.empty())
        return req.server;
    const size_t scheme = req.url.find("://");
    const size_t path = req.url.find('/', scheme == std::string::npos ? 0 : scheme + 3);
    return req.url.substr(0, path);
}

void Pool::submit(Request req, Work work)
{
    m_busy++;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string server = server_of(req);
        m_jobs.push_back({std::move(req), std::move(work), std::move(server), clock_now()});
    }
    m_cv.notify_one();
}

std::deque<Pool::Job>::iterator Pool::next_job(double now)
{
    for (auto it = m_jobs.begin(); it != m_jobs.end(); ++it) {
        const auto down = m_down_until.find(it->server);
        if (down != m_down_until.end() && now < down->second && !it->req.fresh)
            return it;   /* answered at once */
        const auto running = m_running.find(it->server);
        if (running == m_running.end() || running->second < kPerServer)
            return it;
    }
    return m_jobs.end();
}

std::map<std::string, double> Pool::waiting() const
{
    const double now = clock_now();
    std::map<std::string, double> out;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto note = [&](const std::string &server, double since) {
        double &age = out[server];
        age = std::max(age, now - since);
    };
    for (const Job &j : m_jobs)
        note(j.server, j.since);
    for (const auto &kv : m_started)
        note(kv.first, kv.second);
    return out;
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
        bool aside = false;
        std::multimap<std::string, double>::iterator started;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            std::deque<Job>::iterator it;
            m_cv.wait(lock, [this, &it] {
                if (m_stop)
                    return true;
                it = next_job(clock_now());
                return it != m_jobs.end();
            });
            if (m_stop)
                return;
            job = std::move(*it);
            m_jobs.erase(it);
            const auto down = m_down_until.find(job.server);
            aside = !job.req.fresh && down != m_down_until.end() && clock_now() < down->second;
            if (!aside) {
                m_running[job.server]++;
                started = m_started.emplace(job.server, job.since);
            }
        }
        Response resp;
        if (aside) {   /* unreachable a moment ago: not tried again so soon */
            resp.fail = Response::Connect;
            resp.error = "set aside: could not be reached a moment ago";
        } else {
            resp = m_transport.perform(job.req, &m_stop);
        }
        std::function<void()> then = job.work ? job.work(resp) : nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_done.push_back(std::move(then));
            if (!aside) {
                if (--m_running[job.server] <= 0)
                    m_running.erase(job.server);
                m_started.erase(started);
                if (resp.fail == Response::Connect)
                    m_down_until[job.server] = clock_now() + kSetAside;
                else if (resp.fail == Response::None)
                    m_down_until.erase(job.server);   /* it answers: back in use */
            }
        }
        m_busy--;
        m_cv.notify_all();   /* its server has room again */
        if (m_wake)
            m_wake();
    }
}

std::string header(const std::string &name, const std::string &value) { return name + ": " + value; }

std::string size_text(size_t bytes)
{
    char buf[32];
    if (bytes < 1024)
        std::snprintf(buf, sizeof buf, "%zu B", bytes);
    else if (bytes < (1u << 20))
        std::snprintf(buf, sizeof buf, "%zu KiB", (bytes + 512) >> 10);
    else
        std::snprintf(buf, sizeof buf, "%.1f MiB", bytes / 1048576.0);
    return buf;
}

} // namespace http
