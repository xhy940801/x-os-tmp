#pragma once

/* Minimum and maximum values a `signed char' can hold.  */
#undef SCHAR_MIN
#define SCHAR_MIN (-128)
#undef SCHAR_MAX
#define SCHAR_MAX 127

/* Maximum value an `unsigned char' can hold.  (Minimum is 0).  */
#undef UCHAR_MAX
#define UCHAR_MAX 255

/* Minimum and maximum values a `char' can hold.  */
#ifdef __CHAR_UNSIGNED__
#undef CHAR_MIN
#define CHAR_MIN 0
#undef CHAR_MAX
#define CHAR_MAX 255
#else
#undef CHAR_MIN
#define CHAR_MIN (-128)
#undef CHAR_MAX
#define CHAR_MAX 127
#endif

/* Minimum and maximum values a `signed short int' can hold.  */
#undef SHRT_MIN
#define SHRT_MIN (-32768)
#undef SHRT_MAX
#define SHRT_MAX 32767

/* Maximum value an `unsigned short int' can hold.  (Minimum is 0).  */
#undef USHRT_MAX
#define USHRT_MAX 65535

/* Minimum and maximum values a `signed int' can hold.  */
#undef INT_MIN
#define INT_MIN (-2147483648)
#undef INT_MAX
#define INT_MAX 2147483647

/* Maximum value an `unsigned int' can hold.  (Minimum is 0).  */
#undef UINT_MAX
#define UINT_MAX 4294967295

/* Minimum and maximum values a `signed long int' can hold.  */
#undef LONG_MIN
#define LONG_MIN INT_MIN
#undef LONG_MAX
#define LONG_MAX INT_MAX

/* Maximum value an `unsigned long int' can hold.  (Minimum is 0).  */
#undef ULONG_MAX
#define ULONG_MAX UINT_MAX
