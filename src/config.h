#ifndef CMARK_CONFIG_H
#define CMARK_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__has_builtin)
#define __has_builtin(builtin) 0
#endif

#if !defined(__has_include)
#define __has_include(include) 0
#endif

#if __STDC_VERSION__-0 >= 199901l || __has_include(<stdbool.h>)
  #define HAVE_STDBOOL_H
  #include <stdbool.h>
#elif !defined(__cplusplus)
  typedef char bool;
#endif

#if __has_builtin(__builtin_expect)
  #define HAVE___BUILTIN_EXPECT
#endif

#if defined(__GNUC__)
  #define HAVE___ATTRIBUTE__
  #define CMARK_ATTRIBUTE(list) __attribute__(list)
#else
  #define CMARK_ATTRIBUTE(list)
#endif

#ifndef CMARK_INLINE
  #if defined(_MSC_VER) && !defined(__cplusplus)
    #define CMARK_INLINE __inline
  #else
    #define CMARK_INLINE inline
  #endif
#endif

#ifdef __cplusplus
}
#endif

#endif
