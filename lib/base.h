#ifndef SYNC_BASE_H
#define SYNC_BASE_H

#ifdef _MSC_VER
 #define _CRT_SECURE_NO_WARNINGS
 #define _CRT_NONSTDC_NO_DEPRECATE
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
<<<<<<< 15b35c4a0804c875defb786ad67ddb2a97486f82
 #if _MSC_VER < 1900
  #define snprintf _snprintf
=======
 #if _MSC_VER < 1700
   /* I'm keeping this intact for anything below VS 2012 */
   #define strdup _strdup
   #define snprintf _snprintf
>>>>>>> VS 2012+2015 compile fixes (w/o warnings)
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
