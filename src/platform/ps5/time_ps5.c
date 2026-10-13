/*
 * Regie5 — a TV front end for a self-hosted media stack
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Dates on the PS5 without the console's libc time functions (localtime,
 * mktime, time...: a title that runs on consoles, ps5-homebrew-ui, never
 * calls them, and the native runtime may not set up what they rely on). The
 * calendar arithmetic is done here; only the time-zone offset comes from the
 * kernel (sceKernelConvertUtcToLocaltime, libkernel, which the title imports
 * anyway). Defined here, these win over the libc's at link time.
 *
 * The console's clock, as clock_gettime(CLOCK_REALTIME) reads it, was found
 * an hour ahead of UTC on a 13.60 console in summer time, and the kernel's
 * offset left summer time out: the local time came out right, the UTC an
 * hour wrong (and with it every time a server sends). The servers' clock
 * (their Date header) is taken as UTC once known: the clock's lead over it
 * is taken off the clock, and its part in quarter hours (the zone the
 * kernel missed) is added to the offset. What is left (a console without
 * internet has no time server: its clock drifts, a minute here) is
 * corrected too, so the clock shown is the servers'.
 *
 * days_from_civil / civil_from_days: Howard Hinnant's public-domain
 * algorithms (http://howardhinnant.github.io/date_algorithms.html).
 */
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

int sceKernelConvertUtcToLocaltime(time_t utc, time_t *local, void *tz, int *dst_seconds);
int clock_gettime(clockid_t clock, struct timespec *ts);

static int64_t days_from_civil(int64_t y, int64_t m, int64_t d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(int64_t z, int64_t *y, int *m, int *d)
{
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = yoe + era * 400 + (*m <= 2);
}

static int64_t floor_div(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

/* Seconds since the epoch, as a calendar date (UTC arithmetic). */
static void fill(int64_t secs, struct tm *out)
{
    const int64_t days = floor_div(secs, 86400);
    const int64_t rem = secs - days * 86400;
    int64_t y;
    int m, d;
    civil_from_days(days, &y, &m, &d);
    memset(out, 0, sizeof *out);
    out->tm_year = (int)(y - 1900);
    out->tm_mon = m - 1;
    out->tm_mday = d;
    out->tm_hour = (int)(rem / 3600);
    out->tm_min = (int)(rem % 3600 / 60);
    out->tm_sec = (int)(rem % 60);
    out->tm_wday = (int)((days % 7 + 11) % 7);   /* 1970-01-01 was a Thursday */
    out->tm_yday = (int)(days - days_from_civil(y, 1, 1));
}

/* The local offset from UTC at a time, in seconds; asked once an hour. */
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static int64_t s_hour = INT64_MIN, s_offset, s_dst;
/* How far the console's clock runs ahead of the servers' UTC: s_lead to the
 * second (taken off the clock), s_skew its quarter hours (added to the
 * offset); 0 until a server has answered. */
static int64_t s_skew, s_lead;

static int64_t raw_clock(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec;
}

int64_t regie5_utc_offset(time_t when)
{
    const int64_t hour = floor_div((int64_t)when, 3600);
    pthread_mutex_lock(&s_lock);
    if (hour != s_hour) {
        time_t local = when;
        unsigned char tz[64];   /* the kernel's struct timezone, roomy */
        int dst = 0;
        s_offset = sceKernelConvertUtcToLocaltime(when, &local, tz, &dst) == 0 ? (int64_t)(local - when) : 0;
        s_dst = dst;
        s_hour = hour;
    }
    const int64_t offset = s_offset + s_skew;
    pthread_mutex_unlock(&s_lock);
    return offset;
}

time_t time(time_t *out)
{
    pthread_mutex_lock(&s_lock);
    const int64_t lead = s_lead;
    pthread_mutex_unlock(&s_lock);
    const time_t t = (time_t)(raw_clock() - lead);
    if (out)
        *out = t;
    return t;
}

int regie5_note_server_utc(int64_t server_utc, int64_t *zone_part, int64_t *drift_part)
{
    const int64_t lead = raw_clock() - server_utc;
    /* Quarter hours: a zone or summer time the kernel missed; a lead beyond
     * 14 h is a clock set wrong, not a zone: left alone. */
    const int64_t q = lead >= 0 ? (lead + 450) / 900 : -((-lead + 450) / 900);
    if (q * 900 > 14 * 3600 || q * 900 < -14 * 3600)
        return 0;
    pthread_mutex_lock(&s_lock);
    /* The Date header has whole seconds and the answer took a moment: a
     * second either way is noise, not a change. */
    const int changed = q * 900 != s_skew || lead - s_lead > 1 || s_lead - lead > 1;
    if (changed) {
        s_skew = q * 900;
        s_lead = lead;
    }
    *zone_part = s_skew;
    *drift_part = s_lead - s_skew;
    pthread_mutex_unlock(&s_lock);
    return changed;
}

void regie5_time_report(int64_t *raw, int64_t *kernel_offset, int64_t *dst, int64_t *skew)
{
    *raw = raw_clock();
    regie5_utc_offset((time_t)*raw);
    pthread_mutex_lock(&s_lock);
    *kernel_offset = s_offset;
    *dst = s_dst;
    *skew = s_skew;
    pthread_mutex_unlock(&s_lock);
}

struct tm *gmtime_r(const time_t *when, struct tm *out)
{
    fill((int64_t)*when, out);
    return out;
}

struct tm *localtime_r(const time_t *when, struct tm *out)
{
    const int64_t offset = regie5_utc_offset(*when);
    fill((int64_t)*when + offset, out);
    out->tm_gmtoff = (long)offset;
    return out;
}

struct tm *gmtime(const time_t *when)
{
    static struct tm t;
    return gmtime_r(when, &t);
}

struct tm *localtime(const time_t *when)
{
    static struct tm t;
    return localtime_r(when, &t);
}

/* Normalises the fields (a day past the month's end, an hour past 23...) as
 * the C library's does: the code relies on it to step through days. */
time_t mktime(struct tm *tm)
{
    int64_t year = tm->tm_year + 1900LL, mon = tm->tm_mon;
    year += floor_div(mon, 12);
    mon -= floor_div(mon, 12) * 12;
    const int64_t local = (days_from_civil(year, mon + 1, 1) + tm->tm_mday - 1) * 86400 + tm->tm_hour * 3600LL +
                          tm->tm_min * 60LL + tm->tm_sec;
    /* The offset that applies at the UTC time it stands for (twice: a DST edge). */
    int64_t utc = local - regie5_utc_offset((time_t)local);
    utc = local - regie5_utc_offset((time_t)utc);
    localtime_r((const time_t *)&utc, tm);
    return (time_t)utc;
}

time_t timegm(struct tm *tm)
{
    int64_t year = tm->tm_year + 1900LL, mon = tm->tm_mon;
    year += floor_div(mon, 12);
    mon -= floor_div(mon, 12) * 12;
    const int64_t utc = (days_from_civil(year, mon + 1, 1) + tm->tm_mday - 1) * 86400 + tm->tm_hour * 3600LL +
                        tm->tm_min * 60LL + tm->tm_sec;
    fill(utc, tm);
    return (time_t)utc;
}
