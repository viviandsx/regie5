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
 *
 * Each server (Request::server, else its scheme, host and port) has at most
 * kPerServer requests on its way: a slow one never holds every worker, the
 * others go on. One that could not be reached is set aside kSetAside seconds:
 * its requests fail at once meanwhile (Request::fresh still goes).
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <pthread.h>
#include <vector>

namespace http {

/* Requests on their way to one server at once, and how long one that could
 * not be reached is set aside. */
constexpr int kPerServer = 2;
constexpr double kSetAside = 10.0;

/* The stack a worker asks for (more if the system's default is larger). */
constexpr size_t kWorkerStack = 512u << 10;
/* Development: another size for the workers to come (Linux:
 * REGIE5_WORKER_STACK_KIB, to show the work fits in the PS5's 64 KiB). */
void set_worker_stack(size_t bytes);
/* Called first thing in each worker thread (the PS5 notes how far up its
 * stack a crash report may read). Set before the pools are made. */
void set_thread_start(void (*hook)());

struct Request {
    std::string method = "GET";
    std::string url;
    std::vector<std::string> headers;   /* "Name: value" */
    std::string body;                   /* sent as application/json when not empty */
    int timeout_s = 10;
    /* Bytes of body at most (a library list): past it the answer is given up,
     * Response::TooLarge. 0: the transport's own limit (64 MiB). */
    size_t max_body = 0;
    bool insecure = false;
    /* Which server it goes to, for the limit and the setting aside (a
     * service's address); empty: the URL's scheme, host and port. */
    std::string server;
    /* Tried even while its server is set aside (a connection test). */
    bool fresh = false;   /* https: the certificate is not checked (config.json "insecure") */
};

struct Response {
    /* no HTTP answer, and why; Unsupported: this transport cannot reach such an address (https on the PS5) */
    /* Certificate: an https:// server whose certificate is refused; TooLarge:
     * a body past Request::max_body (given up before it fills the memory) */
    enum Fail { None, Connect, Timeout, Other, Unsupported, Certificate, TooLarge } fail = None;
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
    /* For each server with a request on its way (or waiting its turn): the
     * seconds since the oldest was submitted. */
    std::map<std::string, double> waiting() const;
    /* The key a request counts under (Request::server, else from its URL). */
    static std::string server_of(const Request &req);

private:
    struct Job {
        Request req;
        Work work;
        std::string server;
        double since = 0;   /* when submitted (seconds, steady clock) */
    };
    void run();
    /* Under the lock: the first job whose server has room (or is set aside), else end. */
    std::deque<Job>::iterator next_job(double now);
    static void *start(void *pool);

    Transport &m_transport;
    std::function<void()> m_wake;
    std::vector<pthread_t> m_threads;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<Job> m_jobs;
    std::map<std::string, int> m_running;             /* by server */
    std::multimap<std::string, double> m_started;     /* by server: when each running one was submitted */
    std::map<std::string, double> m_down_until;       /* by server: set aside until */
    std::vector<std::function<void()>> m_done;
    std::atomic<bool> m_stop{false};
    std::atomic<int> m_busy{0};
};

/* "Name: value" for the services' auth headers. */
std::string header(const std::string &name, const std::string &value);
/* A body's size for the log: "812 B", "31 KiB", "18.5 MiB". */
std::string size_text(size_t bytes);

} // namespace http
