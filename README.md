# timx

裸机协作式软定时器调度器（C99）：STM32 裸机程序的主循环骨架。

定时器结构由调用者持有（侵入式），调度器零动态内存；时间由驱动喂，
SysTick、RTC、测试里的虚拟时钟都能当节拍源。

## 为什么值得看

- **32 位 tick 回绕安全**：用「无符号差值 + 有符号比较」判定到期，
  时钟走到 2^32 翻回来照样正确——这是嵌入式面试的经典考点，测试里有专门用例
- **欠账不补**：系统卡顿错过多个周期时，周期定时器只补位、不连发，
  防止回调风暴饿死主循环（补位用除法一步到位，O(1)）
- **回调安全**：先摘除/补位、后回调，回调里 stop 任何定时器（包括自杀）、
  start 新定时器都是合法操作
- **低功耗接口**：`timx_next_delay()` 返回距最近一次到期的时间，
  就是「可以放心进 STOP 模式多久」的答案

## 快速开始

```bash
make test    # 或手动编译，见下
```

```bash
gcc -std=c99 -Wall -Wextra -Werror -O2 -Iinclude tests/test_timx.c src/timx.c -o test_timx
./test_timx
```

## 使用示例

```c
#include "timx/timx.h"

static timx_sched_t sched;
static timx_timer_t t_blink, t_sample;

static void blink(void *user)   { toggle_led(); }
static void sample(void *user)  { read_sensor(); }

int main(void)
{
    timx_init(&sched);
    timx_start(&sched, &t_blink,   500, 500, blink,  0); /* 0.5s 闪灯 */
    timx_start(&sched, &t_sample,   10,  10, sample, 0); /* 10ms 采样 */

    while (1) {
        /* SysTick 里每 1ms 喂一次： timx_advance(&sched, 1) */
        if (timx_next_delay(&sched) > 5) {
            enter_stop_mode_5ms(); /* 低功耗：睡到下一个定时器 */
        }
    }
}
```

## 语义要点

| 主题 | 行为 |
| --- | --- |
| 重复 `timx_start` | 等价于重启（隐式 stop 再启动） |
| 回调里 stop 自己 | 合法，本次触发后彻底离场 |
| 回调里 start 新定时器 | 合法，下次 advance 起参与调度 |
| 大步进跨多个周期 | 触发 1 次，deadline 一步补位到未来 |
| 同一时刻多个到期 | 都触发，顺序 = 链表遍历序，不保证优先级 |

## License

MIT
