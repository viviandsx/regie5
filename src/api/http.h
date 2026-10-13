/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * HTTP for the API clients. A Transport performs one blocking request (libcurl
 * on Linux, the fixtures in --mock; on the PS5, phase 6); a Pool runs them on
 * worker threads so the interface never waits. Each job's work (parsing) runs
 * on the worker too; what it hands back runs on the main thread, in pump().
 * Workers get a stack of at least kWorkerStack: the PS5 gives new threads
 * only 64 KiB.
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <pthread.h>
#include <vector>

namespace http {

/* The stack a worker asks for (more if the system's default is larger). */
constexpr size_t kWorkerStack = 512u << 10;
/* Development: another size for the workers to come (Linux:
 * REGIE5_WORKER_STACK_KIB, to show the work fits in the PS5's 64 KiB). */
void set_worker_stack(size_t bytes);

struct Request {
    std::string method = "GET";
    std::string url;
    std::vector<std::string> headers;   /* "Name: value" */
    std::string body;                   /* sent as application/json when not empty */
    int timeout_s = 10;
    bool insecure = false;   /* https: the certificate is not checked (config.json "insecure") */
};

struct Response {
    /* no HTTP answer, and why; Unsupported: this transport cannot reach such an address (https on the PS5) */
    /* Certificate: an https:// server whose certificate is refused */
    enum Fail { None, Connect, Timeout, Other, Unsupported, Certificate } fail = None;
    int status = 0;            /* HTTP status; 0 when no answer */
    std::string body;
    std::string content_type;
    std::string error;         /* the transport's text, for the log */
    bool ok() const { return fail == None && status >= 200 && status < 300; }
};

class Transport {
public:
    virtual ~Transport() = default;
    /* Blocking; called from several worker threads at once. *cancel turns true
     * when the app is closing: a long request may give up. */
    virtual Response perform(const Request &req, const std::atomic<bool> *cancel) = 0;
};

class Pool {
public:
    /* On a worker: turns the response into a result, handed back as the
     * function to run on the main thread. */
    using Work = std::function<std::function<void()>(Response &)>;

    /* wake: called from a worker when a result is ready (the loop may be asleep). */
    Pool(Transport &transport, int threads, std::function<void()> wake);
    ~Pool();

    void submit(Request req, Work work);
    /* On the main thread: runs what finished. Returns how many. */
    int pump();
    /* Requests waiting or running. */
    int busy() const { return m_busy.load(); }

private:
    struct Job {
        Request req;
        Work work;
    };
    void run();
    static void *start(void *pool);

    Transport &m_transport;
    std::function<void()> m_wake;
    std::vector<pthread_t> m_threads;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<Job> m_jobs;
    std::vector<std::function<void()>> m_done;
    std::atomic<bool> m_stop{false};
    std::atomic<int> m_busy{0};
};

/* "Name: value" for the services' auth headers. */
std::string header(const std::string &name, const std::string &value);

} // namespace http
