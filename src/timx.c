#include "timx/timx.h"

/* 到期判定：deadline 落在 now 或过去（含 32 位回绕）。
 * 无符号差值转有符号比较是回绕安全的标准写法：
 * 只要两个时刻相差不超过 ±2^31，结果都正确。 */
static bool due(const timx_sched_t *s, const timx_timer_t *t)
{
    return (int32_t)(t->deadline - s->now) <= 0;
}

void timx_init(timx_sched_t *s)
{
    s->now = 0;
    s->head = 0;
}

void timx_start(timx_sched_t *s, timx_timer_t *t, timx_tick_t delay,
                timx_tick_t period, timx_cb cb, void *user)
{
    timx_stop(s, t); /* 重复 start 等价于重启 */
    t->cb = cb;
    t->user = user;
    t->period = period;
    t->deadline = s->now + delay; /* 无符号加法，回绕自然正确 */
    t->active = true;
    t->next = s->head; /* 头插 */
    s->head = t;
}

void timx_stop(timx_sched_t *s, timx_timer_t *t)
{
    if (!t->active) {
        return;
    }
    timx_timer_t **pp = &s->head;
    while (*pp && *pp != t) {
        pp = &(*pp)->next;
    }
    if (*pp) {
        *pp = t->next;
    }
    t->active = false;
    t->next = 0;
}

timx_tick_t timx_next_delay(const timx_sched_t *s)
{
    timx_tick_t best = TIMX_NO_TIMER;
    for (const timx_timer_t *t = s->head; t; t = t->next) {
        timx_tick_t diff = t->deadline - s->now; /* 无符号差值，回绕安全 */
        if ((int32_t)diff <= 0) {
            return 0; /* 已有到期的 */
        }
        if (diff < best) {
            best = diff;
        }
    }
    return best;
}

void timx_advance(timx_sched_t *s, timx_tick_t elapsed)
{
    s->now += elapsed;

    /* 先摘除/补位、后回调：回调里 stop/start 任何定时器（包括自己）都安全 */
    timx_timer_t *t = s->head;
    while (t) {
        timx_timer_t *next = t->next; /* 回调可能改链表，先存 */
        if (due(s, t)) {
            if (t->period == 0) {
                timx_stop(s, t); /* 单次：先离场再响 */
            } else {
                /* 周期：欠账不补——不管欠了几个周期都只触发一次，
                 * deadline 用除法一步补到未来，防止回调风暴 */
                timx_tick_t missed =
                    (s->now - t->deadline) / t->period + 1u;
                t->deadline += missed * t->period;
            }
            t->cb(t->user);
        }
        t = next;
    }
}
