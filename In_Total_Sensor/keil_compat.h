#ifndef KEIL_COMPAT_H
#define KEIL_COMPAT_H

#ifndef __ASSEMBLER__
#ifdef __GNUC__
  /* Keil 编译器内建关键字 -> GCC 等价 */
  #define __packed          __attribute__((packed))
  #define __weak            __attribute__((weak))
  #define __inline          inline
  #define __forceinline     __attribute__((always_inline)) inline
  #define __align(n)        __attribute__((aligned(n)))
  #define __irq
  #define __pure            __attribute__((pure))
  #ifndef __nop
    #define __nop()         __asm volatile("nop")
  #endif

  /* 强制包含常用标准头，避免 GCC 14+ 对隐式声明报错 */
  #include <stdint.h>
  #include <stddef.h>
  #include <stdbool.h>
  /* itoa 冲突保护：用户若自定义了 itoa，stdlib.h 的声明会冲突 */
  #define itoa __newlib_itoa
  #include <stdlib.h>
  #undef itoa
  #include <string.h>
#endif
#endif /* __ASSEMBLER__ */

#endif /* KEIL_COMPAT_H */