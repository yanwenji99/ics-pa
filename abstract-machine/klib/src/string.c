#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  size_t length = 0;
  while (*s != '\0')
  {
    length++;
    s++;
  }
  return length;
}

char *strcpy(char *dst, const char *src) {
  char *ret = dst;
  while (*src != '\0')
  {
    *dst = *src;
    dst++;
    src++;
  }
  *dst = '\0';
  return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
  char *ret = dst;
  size_t i = 0;
  for(i = 0; i < n && src[i] != '\0'; i++)
  {
    dst[i] = src[i];
  }
  for(; i < n; i++)
  {
    dst[i] = '\0';
  }
  return ret;
}

char *strcat(char *dst, const char *src) {
  char *ret = dst;
  size_t len = strlen(dst);
  while(*src !='\0'){
    dst[len] = *src;
    len++;
    src++;
  }
  dst[len] = '\0';
  return ret;
}

int strcmp(const char *s1, const char *s2) {
  while(*s1 != '\0' && *s2 != '\0')
  {
    if(*s1 != *s2)
      return (unsigned char)*s1 - (unsigned char)*s2;
    s1++;
    s2++;
  }
  return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  size_t i = 0;
  while(i < n && s1[i] != '\0' && s2[i] != '\0')
  {
    if(s1[i] != s2[i])
      return (unsigned char)s1[i] - (unsigned char)s2[i];
    i++;
  }
  if(i == n) return 0;
  return (unsigned char)s1[i] - (unsigned char)s2[i];
}

void *memset(void *s, int c, size_t n) {
  size_t i = 0;
  unsigned char *p = (unsigned char *)s;
  for(i = 0; i < n; i++)
  {
    p[i] = (unsigned char)c;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n)
{ // memmove() 能够保证源串在被覆盖之前将重叠区域的字节拷贝到目标区域中，复制后源区域的内容会被更改。如果目标区域与源区域没有重叠，则和 memcpy() 函数功能相同
  unsigned char *d = (unsigned char *)dst;
  const unsigned char *s = (const unsigned char *)src;
  if (d < s)
  {
    // 从前往后复制
    while (n--)
    {
      *d++ = *s++;
    }
  }
  else
  {
    // 从后往前复制
    d += n;
    s += n;
    while (n--)
    {
      *--d = *--s;
    }
  }
  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  unsigned char *d = (unsigned char *)out;
  const unsigned char *s = (const unsigned char *)in;
  while (n--)
  {
    *d++ = *s++;
  }
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *p1 = (const unsigned char *)s1;
  const unsigned char *p2 = (const unsigned char *)s2;
  while (n--)
  {
    if (*p1 != *p2)
      return (int)(*p1 - *p2);
    p1++;
    p2++;
  }
  return 0;
}

#endif
