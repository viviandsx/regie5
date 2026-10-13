/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * What Mbed TLS asks of the system, from platform.h (mbedtls_config.h says
 * why): randomness and a millisecond clock.
 */
#ifdef REGIE5_TLS

#include "platform/platform.h"

#include "mbedtls/build_info.h"
#include "mbedtls/entropy.h"
#include "mbedtls/platform_time.h"

extern "C" int mbedtls_hardware_poll(void *, unsigned char *output, size_t len, size_t *olen)
{
    if (!platform::random_bytes(output, len)) {
        *olen = 0;
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    *olen = len;
    return 0;
}

extern "C" mbedtls_ms_time_t mbedtls_ms_time(void) { return (mbedtls_ms_time_t)(platform::now() * 1000.0); }

#endif
