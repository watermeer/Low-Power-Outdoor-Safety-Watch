/*==============================================================================
 * 副核链接脚本 — GD32L233CCT6 (Cortex-M23)
 *==============================================================================
 * 芯片: GD32L233CCT6 — Flash 256KB / SRAM 32KB
 * 工具: arm-none-eabi-gcc + GNU ld
 *
 * 移植自官方模板 SDK/.../GCC/Ld/gd32l233xC_flash.ld，
 * 只改了堆栈尺寸和注释 —— 段布局保持官方原样，不自己乱动。
 *
 * startup_gd32l233.S 依赖下面这几个符号，缺一个就起不来：
 *   _sp / _sidata / _sdata / _edata / _sbss / _ebss
 *============================================================================*/

/* 存储器映射 */
MEMORY
{
  FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
  RAM      (xrw) : ORIGIN = 0x20000000, LENGTH = 32K
}

ENTRY(Reset_Handler)

SECTIONS
{
  /* 栈 1K 够了：裸机无 OS，最深调用链是 AlarmMgr → 状态机那几层
   * 堆保留 512B：我们不用 malloc，留着给 newlib 的 printf 兜底 */
  __stack_size = DEFINED(__stack_size) ? __stack_size : 1K;
  __heap_size  = DEFINED(__heap_size)  ? __heap_size  : 512;

  /* ── 中断向量表：必须落在 Flash 最前面 ── */
  .vectors :
  {
    . = ALIGN(4);
    KEEP(*(.vectors))
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

  /* ── 只读数据（阈值表、字体、字符串常量） ── */
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

  /* ── C 运行时初始化数组（__libc_init_array 要用） ── */
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

  /* ── 已初始化变量：运行在 RAM，镜像存在 Flash ──
   * _sidata 是 Flash 里的加载地址，_sdata/_edata 是 RAM 里的运行地址
   * startup 里靠这三个符号把初值搬过去 */
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

  /* ── 未初始化变量：startup 里清成 0 ── */
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

  /* ── 堆栈区（_sp 由 startup 载入 MSP） ── */
  .heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = _ebss );
    PROVIDE ( _end = _ebss );
    . = . + __heap_size;
    PROVIDE( _heap_end = . );
    . = . + __stack_size;
    PROVIDE( _sp = . );
    . = ALIGN(8);
  } > RAM
}

/* 运行时库 */
GROUP(libgcc.a libc.a libm.a libnosys.a)
