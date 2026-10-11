/* Direct observations from the required, unmodified local C sources. */
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <osmocom/core/timer.h>

static void pair(const char *kind, int id, const char *op, long as, long af,
                 long bs, long bf, long rs, long rf, int ret)
{
    printf("{\"type\":\"%s\",\"id\":%d,\"op\":\"%s\",\"as\":%ld,\"af\":%ld,\"bs\":%ld,\"bf\":%ld,\"rs\":%ld,\"rf\":%ld,\"ret\":%d}\n",
           kind, id, op, as, af, bs, bf, rs, rf, ret);
}

static void arithmetic(void)
{
    const long sec[] = {-3, 0, 4};
    const long frac[] = {-2000001, -2000000, -1000001, -1000000, -1,
                        0, 1, 999999, 1000000, 1000001, 2000000, 2000001};
    for (int ns = 0; ns < 2; ns++) {
        int id = 0;
        for (unsigned a = 0; a < 3; a++)
        for (unsigned b = 0; b < 3; b++)
        for (unsigned x = 0; x < 12; x++)
        for (unsigned y = 0; y < 12; y++, id++) {
            /* ns boundary +/-1 is one nanosecond, not 1000 ns. */
            long af = frac[x], bf = frac[y];
            if (ns) {
                af = (af / 1000000) * 1000000000 + af % 1000000;
                bf = (bf / 1000000) * 1000000000 + bf % 1000000;
                if (frac[x] == 999999) af = 999999999;
                if (frac[y] == 999999) bf = 999999999;
            }
            for (int sub = 0; sub < 2; sub++) {
                long rs, rf;
                if (ns) {
                    struct timespec ta = {sec[a], af}, tb = {sec[b], bf}, out = {71, 72};
                    if (sub) timespecsub(&ta, &tb, &out); else timespecadd(&ta, &tb, &out);
                    rs = out.tv_sec; rf = out.tv_nsec;
                } else {
                    struct timeval ta = {sec[a], af}, tb = {sec[b], bf}, out = {71, 72};
                    if (sub) timersub(&ta, &tb, &out); else timeradd(&ta, &tb, &out);
                    rs = out.tv_sec; rf = out.tv_usec;
                }
                pair(ns ? "timespec" : "timeval", id, sub ? "sub" : "add",
                     sec[a], af, sec[b], bf, rs, rf, 0);
            }
            if (ns) {
                struct timespec ta = {sec[a], af}, tb = {sec[b], bf}, out = ta;
                timespecadd(&out, &tb, &out);
                pair("timespec", id, "alias_add", sec[a], af, sec[b], bf, out.tv_sec, out.tv_nsec, 0);
                timespecsub(&ta, &tb, &ta);
                pair("timespec", id, "alias_sub", sec[a], af, sec[b], bf, ta.tv_sec, ta.tv_nsec, 0);
                ta = (struct timespec){sec[a], af};
                pair("timespec", id, "cmp", sec[a], af, sec[b], bf, 0, 0,
                     timespeccmp(&ta, &tb, <) ? -1 : timespeccmp(&ta, &tb, >) ? 1 : 0);
                pair("timespec", id, "isset", sec[a], af, sec[b], bf, 0, 0, !!timespecisset(&ta));
                timespecclear(&ta);
                pair("timespec", id, "clear", sec[a], af, sec[b], bf, ta.tv_sec, ta.tv_nsec, 0);
                struct timespec *slot = osmo_clock_override_gettimespec(CLOCK_MONOTONIC);
                *slot = (struct timespec){sec[a], af};
                osmo_clock_override_add(CLOCK_MONOTONIC, sec[b], bf);
                pair("timespec", id, "override_add", sec[a], af, sec[b], bf, slot->tv_sec, slot->tv_nsec, 0);
            } else {
                struct timeval ta = {sec[a], af}, tb = {sec[b], bf}, out = ta;
                timeradd(&out, &tb, &out);
                pair("timeval", id, "alias_add", sec[a], af, sec[b], bf, out.tv_sec, out.tv_usec, 0);
                timersub(&ta, &tb, &ta);
                pair("timeval", id, "alias_sub", sec[a], af, sec[b], bf, ta.tv_sec, ta.tv_usec, 0);
                ta = (struct timeval){sec[a], af};
                pair("timeval", id, "cmp", sec[a], af, sec[b], bf, 0, 0,
                     timercmp(&ta, &tb, <) ? -1 : timercmp(&ta, &tb, >) ? 1 : 0);
                pair("timeval", id, "isset", sec[a], af, sec[b], bf, 0, 0, !!timerisset(&ta));
                timerclear(&ta);
                pair("timeval", id, "clear", sec[a], af, sec[b], bf, ta.tv_sec, ta.tv_usec, 0);
                osmo_gettimeofday_override_time = (struct timeval){sec[a], af};
                osmo_gettimeofday_override_add(sec[b], bf);
                pair("timeval", id, "override_add", sec[a], af, sec[b], bf,
                     osmo_gettimeofday_override_time.tv_sec, osmo_gettimeofday_override_time.tv_usec, 0);
            }
        }
    }
}

static const clockid_t clocks[] = {CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_PROCESS_CPUTIME_ID,
    CLOCK_THREAD_CPUTIME_ID, CLOCK_MONOTONIC_RAW, CLOCK_REALTIME_COARSE,
    CLOCK_MONOTONIC_COARSE, CLOCK_BOOTTIME};

static void clock_vectors(void)
{
    pair("wall", 0, "initial", 0, 0, 0, 0, osmo_gettimeofday_override_time.tv_sec,
         osmo_gettimeofday_override_time.tv_usec, osmo_gettimeofday_override);
    osmo_gettimeofday_override = true;
    struct timeval tv = {-71, -72};
    struct timezone tz = {73, 74};
    int ret = osmo_gettimeofday(&tv, &tz);
    pair("wall", 0, "read", 0, 0, tz.tz_minuteswest, tz.tz_dsttime, tv.tv_sec, tv.tv_usec, ret);
    osmo_gettimeofday_override_add(-2, 2000001);
    osmo_gettimeofday_override = false;
    osmo_gettimeofday_override = true;
    ret = osmo_gettimeofday(&tv, NULL);
    pair("wall", 0, "retained", -2, 2000001, 0, 0, tv.tv_sec, tv.tv_usec, ret);
    for (unsigned i = 0; i < sizeof(clocks) / sizeof(clocks[0]); i++) {
        struct timespec *slot = osmo_clock_override_gettimespec(clocks[i]);
        pair("clock", clocks[i], "initial", 0, 0, 0, 0, slot->tv_sec, slot->tv_nsec, 0);
        osmo_clock_override_enable(clocks[i], true);
        *slot = (struct timespec){(long)i + 10, -1000000001};
        struct timespec ts = {-71, -72};
        ret = osmo_clock_gettime(clocks[i], &ts);
        pair("clock", clocks[i], "read", slot->tv_sec, slot->tv_nsec, 0, 0, ts.tv_sec, ts.tv_nsec, ret);
        osmo_clock_override_add(clocks[i], -3, 2000000001);
        osmo_clock_override_enable(clocks[i], false);
        pair("clock", clocks[i], "disabled_slot", -3, 2000000001, 0, 0, slot->tv_sec, slot->tv_nsec, 0);
        osmo_clock_override_enable(clocks[i], true);
        ret = osmo_clock_gettime(clocks[i], &ts);
        pair("clock", clocks[i], "retained", 0, 0, 0, 0, ts.tv_sec, ts.tv_nsec, ret);
    }
    /* Observe all slots again after every slot has changed: no aliasing. */
    for (unsigned i = 0; i < sizeof(clocks) / sizeof(clocks[0]); i++) {
        struct timespec ts;
        ret = osmo_clock_gettime(clocks[i], &ts);
        pair("clock", clocks[i], "independent", 0, 0, 0, 0, ts.tv_sec, ts.tv_nsec, ret);
    }
    clockid_t invalid = INT_MAX;
    osmo_clock_override_enable(invalid, true);
    osmo_clock_override_add(invalid, 1, 2);
    struct timespec ts = {71, 72};
    errno = 0;
    ret = osmo_clock_gettime(invalid, &ts);
    pair("clock", invalid, "unsupported", 0, 0, errno,
         osmo_clock_override_gettimespec(invalid) != NULL, ts.tv_sec, ts.tv_nsec, ret);
}

static struct osmo_timer_list timers[4];
static int labels[4] = {0, 1, 2, 3};
static const char *scenario;
static int mode, depth, callbacks;
static void trace(const char *event, int who, int ret)
{
    struct timeval *near = osmo_timers_nearest();
    printf("{\"type\":\"timer\",\"case\":\"%s\",\"event\":\"%s\",\"who\":%d,\"depth\":%d,\"ret\":%d,\"count\":%d,\"active\":%d,\"near\":%d,\"sec\":%ld,\"frac\":%ld}\n",
           scenario, event, who, depth, ret, osmo_timers_check(),
           timers[0].active + 2*timers[1].active + 4*timers[2].active + 8*timers[3].active,
           near != NULL, near ? near->tv_sec : 0, near ? near->tv_usec : 0);
    fflush(stdout);
}
static void replacement(void *data)
{
    trace("replacement", *(int *)data, 0);
}
static void callback(void *data)
{
    int who = *(int *)data;
    if (++callbacks > 12) _Exit(77); /* Bound an unsafe peer-reentrant diagnostic. */
    trace("callback", who, 0);
    if (who != 0) return;
    if (mode == 1 || mode == 2 || mode == 9) {
        if (mode == 2) osmo_timer_schedule(&timers[2], 0, 0);
        depth++;
        trace("nested_enter", who, 0);
        int ret = osmo_timers_update();
        trace("nested_exit", who, ret);
        depth--;
    } else if (mode == 3) {
        osmo_timers_prepare(); trace("prepared", who, osmo_timers_nearest_ms());
        osmo_timer_del(&timers[1]);
        osmo_timers_prepare(); trace("prepared_future", who, osmo_timers_nearest_ms());
    } else if (mode == 4 && callbacks == 1) {
        osmo_timer_schedule(&timers[0], 0, 0);
    } else if (mode == 5) {
        osmo_timer_del(&timers[1]);
    } else if (mode == 6) {
        osmo_timer_schedule(&timers[1], 0, 0);
    } else if (mode == 8) {
        osmo_timer_setup(&timers[0], replacement, &labels[0]);
        osmo_timer_schedule(&timers[0], 0, 0);
    } else if (mode == 10) {
        osmo_timer_setup(&timers[1], replacement, &labels[1]);
    } else if (mode == 7) {
        osmo_timer_schedule(&timers[2], 0, 0);
    }
}
static void timer_case(const char *name, int m)
{
    scenario = name; mode = m; depth = callbacks = 0;
    memset(timers, 0, sizeof(timers));
    osmo_gettimeofday_override = true;
    osmo_gettimeofday_override_time = (struct timeval){100, 0};
    for (int i = 0; i < 4; i++) osmo_timer_setup(&timers[i], callback, &labels[i]);
    if (m == 0) {
        osmo_timer_schedule(&timers[0], 0, 0);
        osmo_timer_schedule(&timers[1], 0, 0);
        osmo_timer_schedule(&timers[2], -1, 0);
    } else {
        if (m == 3 || m == 5 || m == 6 || m == 9 || m == 10) osmo_timer_schedule(&timers[1], 0, 0);
        osmo_timer_schedule(&timers[0], 0, 0);
        if (m == 3) osmo_timer_schedule(&timers[3], 1, 1);
    }
    osmo_timers_prepare(); trace("before", -1, osmo_timers_nearest_ms());
    int ret = osmo_timers_update(); trace("after", -1, ret);
    ret = osmo_timers_update(); trace("second", -1, ret);
    for (int i = 0; i < 4; i++) osmo_timer_del(&timers[i]);
    osmo_timers_prepare(); trace("clean", -1, osmo_timers_nearest_ms());
}


static void timer_api(void)
{
    struct osmo_timer_list timer = {0};
    struct timeval now = {100, 500000}, out;
    osmo_gettimeofday_override_time = now;
    const struct timeval values[] = {
        {100, 500000}, {100, 499999}, {100, 500001}, {99, 1500000},
        {101, -500000}, {0, 2000001}, {-1, -2000001}, {LONG_MAX, 0}
    };
    for (unsigned i = 0; i < sizeof(values)/sizeof(values[0]); i++) {
        timer.timeout = values[i];
        int ret = osmo_timer_remaining(&timer, &now, &out);
        pair("timer_api", i, "remaining", values[i].tv_sec, values[i].tv_usec,
             now.tv_sec, now.tv_usec, out.tv_sec, out.tv_usec, ret);
        ret = osmo_timer_remaining(&timer, NULL, &out);
        pair("timer_api", i, "remaining_clock", values[i].tv_sec, values[i].tv_usec,
             now.tv_sec, now.tv_usec, out.tv_sec, out.tv_usec, ret);
        osmo_timer_schedule(&timer, (int)(i == 7 ? 0 : values[i].tv_sec), (int)values[i].tv_usec);
        pair("timer_api", i, "schedule", i == 7 ? 0 : values[i].tv_sec, values[i].tv_usec,
             now.tv_sec, now.tv_usec, timer.timeout.tv_sec, timer.timeout.tv_usec, osmo_timer_pending(&timer));
        osmo_timer_del(&timer);
    }
    const struct timeval delays[] = {{0,0},{0,1},{0,999},{0,1000},{0,1001},{1,1},{2147483,647000},{0,2000001}};
    for (unsigned i = 0; i < sizeof(delays)/sizeof(delays[0]); i++) {
        timeradd(&now, &delays[i], &timer.timeout);
        osmo_timer_add(&timer);
        osmo_timers_prepare();
        struct timeval *near = osmo_timers_nearest();
        pair("timer_api", i, "nearest", timer.timeout.tv_sec, timer.timeout.tv_usec,
             now.tv_sec, now.tv_usec, near->tv_sec, near->tv_usec, osmo_timers_nearest_ms());
        osmo_timer_del(&timer);
    }
    scenario = "absolute_lifecycle"; depth = 0;
    memset(timers, 0, sizeof(timers));
    osmo_timers_prepare(); trace("empty", -1, osmo_timers_nearest_ms());
    timers[0].timeout = (struct timeval){LONG_MAX,0};
    osmo_timer_add(&timers[0]); osmo_timers_prepare(); trace("max_nearest", -1, osmo_timers_nearest_ms());
    osmo_gettimeofday_override_time = (struct timeval){LONG_MAX,0};
    trace("max_dispatch", -1, osmo_timers_update());
    osmo_gettimeofday_override_time = now;
    timers[0].timeout = (struct timeval){101,500001};
    osmo_timer_add(&timers[0]); osmo_timers_prepare(); trace("future", -1, osmo_timers_nearest_ms());
    osmo_timer_add(&timers[0]); trace("readd", -1, osmo_timers_nearest_ms());
    osmo_timer_del(&timers[0]); osmo_timer_del(&timers[0]); trace("deleted_cache", -1, osmo_timers_nearest_ms());
    timers[0].timeout = now; osmo_timer_add(&timers[0]);
    trace("null_dispatch", -1, osmo_timers_update());
    osmo_timers_prepare(); trace("clean", -1, osmo_timers_nearest_ms());
}

int main(int argc, char **argv)
{
    if (sizeof(time_t) != 8 || sizeof(suseconds_t) != 8 || sizeof(long) != 8 ||
        sizeof(int) != 4 || CHAR_BIT != 8) return 65;
    if (argc == 2 && !strncmp(argv[1], "overflow-", 9)) {
        volatile long high = LONG_MAX, low = LONG_MIN;
        struct timeval a = {high, 0}, b = {1, 0}, out;
        if (!strcmp(argv[1], "overflow-sub")) { a.tv_sec = low; timersub(&a, &b, &out); }
        else if (!strcmp(argv[1], "overflow-carry")) { b = (struct timeval){0, 1000000}; timeradd(&a, &b, &out); }
        else if (!strcmp(argv[1], "overflow-borrow")) { a = (struct timeval){low, 0}; b = (struct timeval){0, 1}; timersub(&a, &b, &out); }
        else if (!strcmp(argv[1], "overflow-fraction")) { a = (struct timeval){0, high}; b = (struct timeval){0, 1}; timeradd(&a, &b, &out); }
        else { timeradd(&a, &b, &out); }
        return (int)out.tv_sec;
    }
    if (argc == 2 && !strcmp(argv[1], "peer-reentrant")) {
        timer_case("peer_reentrant_diagnostic", 9); return 0;
    }
    if (argc != 3) return 64;
    printf("{\"type\":\"meta\",\"schema\":1,\"evidence\":\"exact-source\",\"revision\":\"unknown\",\"reference\":\"%s\",\"generator\":\"%s\",\"time_bits\":64,\"long_bits\":64,\"int_bits\":32,\"embedded\":0}\n", argv[1], argv[2]);
    clock_vectors(); arithmetic();
    const long seconds[] = {LONG_MIN, -31536001, -86401, -3601, -61, -1, 0, 1, 59, 60, 3599, 3600, 86399, 86400, 31536000, LONG_MAX};
    for (unsigned i = 0; i < sizeof(seconds)/sizeof(seconds[0]); i++) {
        long value = seconds[i];
        pair("seconds", i, "sec2min", value, 0, 0, 0, OSMO_SEC2MIN(value), 0, 0);
        pair("seconds", i, "sec2hrs", value, 0, 0, 0, OSMO_SEC2HRS(value), 0, 0);
        pair("seconds", i, "sec2day", value, 0, 0, 0, OSMO_SEC2DAY(value), 0, 0);
    }
    timer_case("batch_order", 0);
    timer_case("nested_empty", 1);
    timer_case("nested_immediate", 2);
    timer_case("prepare_during_dispatch", 3);
    timer_case("self_reschedule", 4);
    timer_case("cancel_peer", 5);
    timer_case("reschedule_peer", 6);
    timer_case("immediate_deferred", 7);
    timer_case("replace_self", 8);
    timer_case("replace_peer", 10);
    timer_api();
    return 0;
}
