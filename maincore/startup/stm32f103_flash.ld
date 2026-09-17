/*==============================================================================
 * 主核链接脚本 — STM32F103C8 (Cortex-M3)
 *==============================================================================
 * 芯片: STM32F103C8T6 — Flash 64KB / SRAM 20KB
 * 工具: arm-none-eabi-gcc + GNU ld
 *
 * ⚠️ 64KB Flash 是硬约束：主核本来就"极简"，
 *    加字库/中文显示之前先跑 size 看看还剩多少。
 *
 * startup_stm32f103_md.S 依赖的符号：
 *   _estack / _sidata / _sdata / _edata / _sbss / _ebss
 *============================================================================*/

/* 存储器映射 */
MEMORY
{
  FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 64K
  RAM      (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
}

/* 栈顶：Cortex-M3 从高地址往下压栈，所以放 RAM 末尾 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

ENTRY(Reset_Handler)

SECTIONS
{
  /* 主核任务型执行，调用链很浅（main→状态机→HAL），1K 栈富余 */
  __stack_size = DEFINED(__stack_size) ? __stack_size : 1K;
  __heap_size  = DEFINED(__heap_size)  ? __heap_size  : 512;

  /* ── 中断向量表 ──
   * ⚠️ 段名是 .isr_vector 不是 .vectors！
   *    ST 官方 startup 里写的是 .isr_vector，GD32 那份才是 .vectors，
   *    抄错段名的后果：向量表被当普通数据塞到后面，
   *    上电从 0x08000000 取到的复位向量是垃圾 → 直接跑飞。
   *    改完一定 objdump -s -j .isr_vector 确认头两个字。 */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  /* ── 代码段 ── */
  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  /* ── 只读数据（OLED 字模放这里，占 Flash 大头） ── */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  .ARM :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .ARM.attributes :
  {
    *(.ARM.attributes)
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(.fini_array*))
    KEEP (*(SORT(.fini_array.*)))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* ── 已初始化变量：Flash 镜像 → RAM 运行 ── */
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT> FLASH

  /* ── 未初始化变量 ── */
  . = ALIGN(4);
  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } > RAM

  /* ── 堆栈区 ──
   * 注意：主核栈顶是 _estack（不像副核用 _sp），
   * 这个区只是把剩余 RAM 显式占住，防止链接器把别的段塞进来 */
  .heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = _ebss );
    PROVIDE ( _end = _ebss );
    . = . + __heap_size;
    PROVIDE( _heap_end = . );
    . = . + __stack_size;
    . = ALIGN(8);
  } > RAM
}

/* 运行时库 */
GROUP(libgcc.a libc.a libm.a libnosys.a)
