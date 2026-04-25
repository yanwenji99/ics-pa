#include <am.h>
#include <klib.h>
#include <klib-macros.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)
static unsigned long int next = 1;

int rand(void)
{
  // RAND_MAX assumed to be 32767
  next = next * 1103515245 + 12345;
  return (unsigned int)(next / 65536) % 32768;
}

void srand(unsigned int seed)
{
  next = seed;
}

int abs(int x)
{
  return (x < 0 ? -x : x);
}

int atoi(const char *nptr)
{
  int x = 0;
  while (*nptr == ' ')
  {
    nptr++;
  }
  while (*nptr >= '0' && *nptr <= '9')
  {
    x = x * 10 + *nptr - '0';
    nptr++;
  }
  return x;
}

// 全局堆顶指针，0 表示尚未初始化
static uintptr_t klib_brk = 0;

void *malloc(size_t size)
{
  // On native, malloc() will be called during initializaion of C runtime.
  // Therefore do not call panic() here, else it will yield a dead recursion:
  //   panic() -> putchar() -> (glibc) -> malloc() -> panic()
  if (size == 0)
  {
    return NULL;
  }

  if (heap.start == NULL || heap.end == NULL)
  {
    return NULL;
  }

  const uintptr_t align = sizeof(uintptr_t);
  const uintptr_t heap_end = (uintptr_t)heap.end;

  if (klib_brk == 0)
  {
    klib_brk = ROUNDUP(heap.start, align);
  }

  uintptr_t p = ROUNDUP(klib_brk, align);
  if (p > heap_end)
  {
#if !(defined(__ISA_NATIVE__) && defined(__NATIVE_USE_KLIB__))
    panic("Out of memory");
#endif
    return NULL;
  }

  if ((uintptr_t)size > heap_end - p)
  {
#if !(defined(__ISA_NATIVE__) && defined(__NATIVE_USE_KLIB__))
    panic("Out of memory");
#endif
    return NULL;
  }

  uintptr_t next = ROUNDUP(p + size, align);
  if (next < p || next > heap_end)
  {
#if !(defined(__ISA_NATIVE__) && defined(__NATIVE_USE_KLIB__))
    panic("Out of memory");
#endif
    return NULL;
  }

  klib_brk = next;
  return (void *)p;
}

void free(void *ptr)
{
}

#endif
