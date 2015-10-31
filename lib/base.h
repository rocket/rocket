#ifndef SYNC_BASE_H
#define SYNC_BASE_H

/* ensure VS shuts up about supposedly deprecated CRT functions */
#ifdef _MSC_VER
 #define _CRT_SECURE_NO_WARNINGS
#endif

#include <stddef.h>

/* configure inline keyword */
#if (!defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)) && !defined(__cplusplus)
 #if defined(_MSC_VER) || defined(__GNUC__) || defined(__SASC)
  #define inline __inline
 #else
  /* compiler does not support inline */
  #define inline
 #endif
#endif

/* configure lacking CRT features */
#ifdef _MSC_VER
 #if _MSC_VER < 1700
   /* I'm keeping this intact for anything below VS 2012 */
   #define strdup _strdup
   #define snprintf _snprintf
 #endif
 /* int is 32-bit for both x86 and x64 */
 typedef unsigned int uint32_t;
 #define UINT32_MAX UINT_MAX
#elif defined(__GNUC__)
 #include <stdint.h>
#elif defined(M68000)
 typedef unsigned int uint32_t;
#endif

#endif /* SYNC_BASE_H */
