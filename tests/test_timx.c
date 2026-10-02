/* timx 单元测试：虚拟时钟驱动，不需要真等待。 */

#include <stdio.h>

#include "timx/timx.h"

static int failed = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
            failed++;                                                  \
        }                                                              \
    } while (0)

/* 记录触发序列 */
#define MAX_EVENTS 64
static int fired_count;
static const void *fired[MAX_EVENTS];

static void note(void *user)
{
    if (fired_count < MAX_EVENTS) {
        fired[fired_count] = user;
    }
    fired_count++;
}

static void reset_events(void)
{
    fired_count = 0;
}

static void test_single_shot(void)
{
    timx_sched_t s;
    timx_timer_t t;
    timx_init(&s);
    reset_events();

    timx_start(&s, &t, 10, 0, note, (void *)"once");
    CHECK(t.active);
    timx_advance(&s, 5);
    CHECK(fired_count == 0); /* 还没到 */
    timx_advance(&s, 4);
    CHECK(fired_count == 0); /* 9 个 tick，还差 1 */
    timx_advance(&s, 1);
    CHECK(fired_count == 1);
    CHECK(!t.active); /* 单次触发后自动离场 */
    timx_advance(&s, 100);
    CHECK(fired_count == 1); /* 不再触发 */
}

static void test_periodic(void)
{
    timx_sched_t s;
    timx_timer_t t;
    timx_init(&s);
    reset_events();

    timx_start(&s, &t, 10, 10, note, (void *)"tick");
    for (int i = 0; i < 5; i++) {
        timx_advance(&s, 10);
    }
    CHECK(fired_count == 5);
    CHECK(t.active);
}

static void test_big_step_debt_not_repaid(void)
{
    /* period=10，一次跨 35 个 tick：欠账不补，只触发 1 次 */
    timx_sched_t s;
    timx_timer_t t;
    timx_init(&s);
    reset_events();

    timx_start(&s, &t, 10, 10, note, (void *)"p");
    timx_advance(&s, 35);
    CHECK(fired_count == 1);
    CHECK(t.active);
    /* 之后按新节拍继续：deadline 已补位到 now 之后 */
    reset_events();
    timx_advance(&s, 10);
    CHECK(fired_count == 1);
}

static void test_stop(void)
{
    timx_sched_t s;
    timx_timer_t t;
    timx_init(&s);
    reset_events();

    timx_start(&s, &t, 10, 0, note, (void *)"x");
    timx_stop(&s, &t);
    CHECK(!t.active);
    timx_advance(&s, 100);
    CHECK(fired_count == 0);
    CHECK(timx_next_delay(&s) == TIMX_NO_TIMER);
}

static void test_restart_reuse(void)
{
    timx_sched_t s;
    timx_timer_t t;
    timx_init(&s);
    reset_events();

    timx_start(&s, &t, 5, 0, note, (void *)"a");
    timx_advance(&s, 5);
    CHECK(fired_count == 1);
    CHECK(!t.active);

    timx_start(&s, &t, 5, 0, note, (void *)"b"); /* 复用同一个结构 */
    CHECK(t.active);
    timx_advance(&s, 5);
    CHECK(fired_count == 2);
}

static void test_multiple_order(void)
{
    timx_sched_t s;
    timx_timer_t a, b;
    timx_init(&s);
    reset_events();

    timx_start(&s, &a, 10, 0, note, (void *)&a);
    timx_start(&s, &b, 5, 0, note, (void *)&b);
    timx_advance(&s, 10);
    CHECK(fired_count == 2);
    CHECK(fired[0] == (void *)&b); /* 先到期的先响 */
    CHECK(fired[1] == (void *)&a);
}

static void test_tick_wraparound(void)
{
    /* 32 位回绕：把时钟推到 2^32 附近再启动定时器 */
    timx_sched_t s;
    timx_timer_t t;
    timx_init(&s);
    reset_events();

    timx_advance(&s, 0xFFFFFFF0u); /* now 离溢出只剩 15 */
    timx_start(&s, &t, 0x20u, 0x20u, note, (void *)"wrap");
    timx_advance(&s, 0x21u); /* 跨过 2^32 边界 */
    CHECK(fired_count == 1);
    reset_events();
    timx_advance(&s, 0x20u);
    CHECK(fired_count == 1); /* 回绕后周期照常 */
    CHECK(s.now < 0x000000FFu); /* 确认真的是回绕后的 now */
}

static void test_next_delay(void)
{
    timx_sched_t s;
    timx_timer_t a, b;
    timx_init(&s);

    CHECK(timx_next_delay(&s) == TIMX_NO_TIMER); /* 空调度器 */

    timx_start(&s, &a, 10, 0, note, (void *)&a);
    timx_start(&s, &b, 3, 0, note, (void *)&b);
    CHECK(timx_next_delay(&s) == 3); /* 最近的是 b */

    timx_advance(&s, 2);
    CHECK(timx_next_delay(&s) == 1);
    timx_advance(&s, 1); /* b 到期离场 */
    CHECK(timx_next_delay(&s) == 7); /* 只剩 a：deadline=10，now=3 */
}

static timx_timer_t self_timer;

static void cb_stop_self(void *user)
{
    timx_sched_t *s = (timx_sched_t *)user;
    timx_stop(s, &self_timer);
    fired_count++;
}

static void test_self_stop(void)
{
    timx_sched_t s;
    timx_init(&s);
    fired_count = 0;

    timx_start(&s, &self_timer, 5, 5, cb_stop_self, (void *)&s);
    timx_advance(&s, 5);  /* 第一次触发，回调里自杀 */
    CHECK(fired_count == 1);
    CHECK(!self_timer.active);
    timx_advance(&s, 50); /* 之后永不再触发 */
    CHECK(fired_count == 1);
}

int main(void)
{
    test_single_shot();
    test_periodic();
    test_big_step_debt_not_repaid();
    test_stop();
    test_restart_reuse();
    test_multiple_order();
    test_tick_wraparound();
    test_next_delay();
    test_self_stop();

    if (failed) {
        printf("FAILED: %d check(s)\n", failed);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
