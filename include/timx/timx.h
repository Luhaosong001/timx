#ifndef TIMX_TIMX_H
#define TIMX_TIMX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* timx：裸机协作式软定时器调度器。
 *
 * - 侵入式：timx_timer_t 由调用者持有，调度器零动态内存
 * - 时间由驱动喂：SysTick / RTC / 测试里的虚拟时钟都行
 * - 32 位 tick 回绕安全（无符号差值比较，见 README）
 * - 周期定时器「欠账不补」：回调风暴时只补位不连发，防止饿死主循环
 * - timx_next_delay() 给低功耗模式算「还能睡多久」
 */

typedef uint32_t timx_tick_t;
#define TIMX_NO_TIMER UINT32_MAX

typedef void (*timx_cb)(void *user);

typedef struct timx_timer {
    timx_cb cb;
    void *user;
    timx_tick_t deadline;    /* 下一次到期的绝对时刻 */
    timx_tick_t period;      /* 周期 tick 数，0 表示单次 */
    bool active;             /* 是否挂在调度器上 */
    struct timx_timer *next; /* 调度器内部链表，勿动 */
} timx_timer_t;

typedef struct {
    timx_tick_t now;         /* 当前虚拟时刻，由 timx_advance 推进 */
    timx_timer_t *head;      /* 活动定时器链表（无序） */
} timx_sched_t;

/* 初始化调度器 */
void timx_init(timx_sched_t *s);

/* 启动定时器：delay 个 tick 后首次触发；period 非 0 则周期触发。
 * t 已在运行时调用会先隐式 stop 再重启。 */
void timx_start(timx_sched_t *s, timx_timer_t *t, timx_tick_t delay,
                timx_tick_t period, timx_cb cb, void *user);

/* 停止定时器；没在运行也无害。回调里 stop 自己是合法操作。 */
void timx_stop(timx_sched_t *s, timx_timer_t *t);

/* 时间流逝 elapsed 个 tick，随后触发所有到期回调（协作式：在调用点执行） */
void timx_advance(timx_sched_t *s, timx_tick_t elapsed);

/* 距最近一次到期还差多少 tick；没有活动定时器返回 TIMX_NO_TIMER。
 * 低功耗场景：这个值就是「可以放心睡多久」的上限。 */
timx_tick_t timx_next_delay(const timx_sched_t *s);

#endif
