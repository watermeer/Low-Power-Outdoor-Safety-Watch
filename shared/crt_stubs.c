/* crt_stubs.c — C 运行时胶水，补 _init/_fini（不是业务代码，别往里加功能）
 *
 * 为什么非要有这个文件：
 *   两个 Makefile 都上了 -nostartfiles（裸机必须的，不然 crt0 会跟 startup
 *   里的 Reset_Handler 打架）。可 newlib 的 __libc_init_array() 里会调一次
 *   _init()，而 _init/_fini 本来是 crti.o 提供的 —— -nostartfiles 把 crti.o
 *   也一起踢了，链接直接报 undefined reference to `_init'。
 *
 *   那就自己补两个空的。反正只会被调一次，什么都不用做。
 *
 * 踩坑记录：
 *   最开始想在链接脚本里写 PROVIDE(_init = 0) 蒙过去，编译是过了，
 *   一上电就 HardFault —— 那是跳到地址 0 执行，不是"空函数"。
 *   这种符号必须给真的函数体。
 */

#include <stddef.h>

/* ★ 设计关键点：-nostartfiles 补钩子 */
/* 被 __libc_init_array() 调，我们没 C++ 全局构造，没事可做 */
void _init(void)
{
    /* 空实现：别删，链接器要这个符号 */
}

/* ★ 设计关键点：真函数体，跳0会HardFault */
/* 裸机没有 exit 路径走不到这儿，但 newlib 引用了这个符号 */
void _fini(void)
{
    /* 空实现：别删，链接器要这个符号 */
}
