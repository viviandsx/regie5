/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "platform/ps5/log_udp.h"

#include "platform/ps5/system_ps5.h"

#include <sys/socket.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

extern "C" int sceKernelUsleep(unsigned int microseconds);

namespace ps5 {

void start_udp_mirror(const char *log_path)
{
    if (log_socket() < 0 || !log_path)
        return;
    const std::string path = log_path;
    std::thread([path] {
        long done = 0;      /* bytes of the file already looked at */
        std::string part;   /* a line not finished yet */
        char buf[4096];
        for (;;) {
            if (FILE *f = std::fopen(path.c_str(), "rb")) {
                std::fseek(f, done, SEEK_SET);
                size_t n;
                while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
                    done += (long)n;
                    part.append(buf, n);
                    size_t nl;
                    while ((nl = part.find('\n')) != std::string::npos) {
                        const std::string line = part.substr(0, nl + 1);
                        part.erase(0, nl + 1);
                        if (line.rfind("[regie5]", 0) != 0)   /* ps5::log sent those already */
                            sendto(log_socket(), line.data(), line.size(), 0,
                                   static_cast<const sockaddr *>(log_address()), log_address_size());
                    }
                }
                std::fclose(f);
            }
            sceKernelUsleep(200000);
        }
    }).detach();
}

} // namespace ps5
