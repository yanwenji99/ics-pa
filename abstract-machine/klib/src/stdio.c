#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

typedef struct __printf_buffer
{
  char *buffer; // 缓冲区
  size_t size;
  size_t pos;
  size_t total;
} p_buf;

const char *parse_format(p_buf *buf, const char *fmt, va_list *ap);
void write_char(p_buf *buf, char c);
void format_int(p_buf *buf, int value);
void format_str(p_buf *buf, const char *str);

void out_buffer(p_buf *buf)
{
  // 从缓冲区读取并输出每个字符
  int i = 0;
  while (buf->buffer[i] != '\0' && i < buf->pos)
  {
    putch(buf->buffer[i++]);
  }
}

int vsnprintf_internal(char *out, size_t n, const char *fmt, va_list ap)
{
  if (n == 0)
    return 0;
  p_buf buf = {
      .buffer = out,
      .size = n,
      .pos = 0,
      .total = 0}; // 初始化缓冲区结构体
  while (*fmt != '\0' && buf.total < __INT_MAX__)
  {
    if (*fmt != '%')
    {
      write_char(&buf, *fmt); // 直接写入普通字符
      fmt++;
      continue;
    }
    fmt = parse_format(&buf, fmt, &ap); // 解析格式字符串
  }
  if (buf.pos < buf.size)
  {
    out[buf.pos] = '\0';
  }
  else if (buf.size > 0)
  {
    out[buf.size - 1] = '\0';
  } // 确保字符串以'\0'结尾
  return buf.total;
}

void write_char(p_buf *buf, char c)
{
  if (buf->pos < buf->size - 1) // 确保有空间写入字符
  {
    buf->buffer[buf->pos++] = c; // 写入字符并更新位置
  }
  buf->total++; // 更新总写入字符数
}

void format_int(p_buf *buf, int value)
{
  // 将整数转换为字符串，并写入缓冲区
  char temp[12];
  int pos = 0;
  if (value < 0)
  {
    write_char(buf, '-');
    value = -value;
  }
  else if (value == 0)
  {
    write_char(buf, '0');
    return;
  }
  while (value > 0)
  {
    temp[pos++] = (value % 10) + '0';
    value /= 10;
  }
  for (int i = pos - 1; i >= 0; i--)
  {
    write_char(buf, temp[i]);
  }
}

void format_str(p_buf *buf, const char *str)
{
  // 将字符串写入缓冲区
  while (*str)
  {
    write_char(buf, *str++);
  }
}

const char *parse_format(p_buf *buf, const char *fmt, va_list *ap)
{
  // 解析格式字符串，处理不同的格式说明符
  // 根据格式说明符从 va_list 中获取对应的参数，并调用 write_char 写入缓冲区
  fmt++; // 跳过 '%' 字符

  // 处理格式说明符
  switch (*fmt)
  {
  case 'd':
    format_int(buf, va_arg(*ap, int));
    break;
  case 's':
    format_str(buf, va_arg(*ap, const char *));
    break;
  }

  return fmt + 1; // 返回下一个字符的位置
}

int printf(const char *fmt, ...)
{
  panic("Not implemented");
}

int vsprintf(char *out, const char *fmt, va_list ap)
{
  return vsnprintf(out, __INT_MAX__, fmt, ap);
}

int sprintf(char *out, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  int result = vsprintf(out, fmt, args);
  va_end(args);
  return result;
}

int snprintf(char *out, size_t n, const char *fmt, ...)
{
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap)
{
  if (n == 0)
    return 0;
  if (n > __INT_MAX__)
    n = __INT_MAX__;
  return vsnprintf_internal(out, n, fmt, ap);
}

#endif
