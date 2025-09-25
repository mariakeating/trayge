#include <stdint.h>
#include <stdbool.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s32 b32;

typedef float f32;
typedef double f64;

#define function static
#define global static

#define Million 1000000
#define Billion 1000000000

#define Assert(condition) do{ if(!(condition)){ asm volatile("int3"); } }while(0)
#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: { InvalidCodePath; } break;

#define CatStringPreProc_(a, b) a##b
#define CatStringPreProc(a, b) CatStringPreProc_(a, b)
#define DeferLoop(begin, end) for(int CatStringPreProc(_i_, __LINE__) = ((begin), 0); !CatStringPreProc(_i_, __LINE__); CatStringPreProc(_i_, __LINE__) += 1, (end))
