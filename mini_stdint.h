#ifndef __MINI_STDINT__
#define __MINI_STDINT__

    /* exact-width signed integer types */
typedef   signed          char int8_t;
typedef   signed short     int int16_t;
typedef   signed           int int32_t;
typedef   signed       __int64 int64_t;

    /* exact-width unsigned integer types */
typedef unsigned          char uint8_t;
typedef unsigned short     int uint16_t;
typedef unsigned           int uint32_t;
typedef unsigned       __int64 uint64_t;

typedef unsigned           int size_t;

#define bool int
#define false 0
#define true 1
#define FALSE 0
#define TRUE 1

#endif // __MINI_STDINT__