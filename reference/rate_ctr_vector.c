/* Direct calls only: no counter behavior is reimplemented here. */
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <osmocom/core/rate_ctr.h>
_Static_assert(sizeof(long) == 8 && sizeof(int) == 4, "recorded Linux ABI");
_Static_assert(RATE_CTR_INTV_NUM == 4, "four intervals");
static void state(const struct rate_ctr *c)
{
    printf("\t%" PRIu64 "\t%" PRIu64, c->current, c->previous);
    for (unsigned i = 0; i < RATE_CTR_INTV_NUM; i++)
        printf("\t%" PRIu64 "\t%" PRIu64, c->intv[i].last, c->intv[i].rate);
}
static void observe(unsigned scenario, unsigned step, const char *op, int inc, struct rate_ctr *c)
{
    struct rate_ctr before = *c;
    int64_t result = 0;
    if (!strcmp(op, "add")) rate_ctr_add(c, inc);
    else if (!strcmp(op, "inc")) rate_ctr_inc(c);
    else if (!strcmp(op, "difference")) result = rate_ctr_difference(c);
    else if (!strcmp(op, "reset")) rate_ctr_reset(c);
    printf("%u\t%u\t%s\t%d\t%" PRId64, scenario, step, op, inc, result);
    state(&before); state(c); putchar('\n');
}
int main(int argc, char **argv)
{
    if (argc == 2) {
        if (!strcmp(argv[1], "null-add")) rate_ctr_add(NULL, 1);
        else if (!strcmp(argv[1], "null-inc")) rate_ctr_inc(NULL);
        else if (!strcmp(argv[1], "null-difference")) return (int)rate_ctr_difference(NULL);
        else if (!strcmp(argv[1], "null-reset")) rate_ctr_reset(NULL);
        else return 2;
        return 0;
    }
    if (argc != 3) return 2;
    printf("meta\t1\texact-source\tunknown\t%s\t%s\t64\t64\t32\n", argv[1], argv[2]);
    const uint64_t values[] = {0, 1, 2, INT_MAX, (uint64_t)INT_MAX + 1,
        INT64_MAX, (uint64_t)INT64_MAX + 1, (uint64_t)INT64_MAX + 2, UINT64_MAX - 1, UINT64_MAX};
    const int increments[] = {0, 1, -1, 2, -2, INT_MIN, INT_MAX};
    unsigned scenario = 0;
    for (unsigned a = 0; a < 10; a++) for (unsigned b = 0; b < 10; b++) {
        struct rate_ctr c = {.current = values[a], .previous = values[b]};
        for (unsigned i = 0; i < 4; i++) {
            c.intv[i].last = values[(a + b + i) % 10];
            c.intv[i].rate = values[(a + b + i + 5) % 10];
        }
        unsigned step = 0;
        ++scenario;
        observe(scenario, step++, "difference", 0, &c);
        observe(scenario, step++, "difference", 0, &c);
        for (unsigned i = 0; i < 7; i++) observe(scenario, step++, "add", increments[i], &c);
        observe(scenario, step++, "inc", 0, &c);
        observe(scenario, step++, "difference", 0, &c);
        observe(scenario, step++, "difference", 0, &c);
        observe(scenario, step++, "reset", 0, &c);
        observe(scenario, step++, "difference", 0, &c);
        observe(scenario, step++, "add", -1, &c);
        observe(scenario, step++, "difference", 0, &c);
        observe(scenario, step++, "reset", 0, &c);
    }
    return 0;
}
