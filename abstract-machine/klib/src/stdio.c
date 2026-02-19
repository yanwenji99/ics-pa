#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

typedef struct {
  char *buf;
  size_t size;
  size_t pos;
  int to_console;
} out_ctx_t;

static void out_char(out_ctx_t *ctx, char ch) {
  if (ctx->to_console) {
    putch(ch);
  }
  if (ctx->buf != NULL && ctx->pos + 1 < ctx->size) {
    ctx->buf[ctx->pos] = ch;
  }
  ctx->pos++;
}

static void out_repeat(out_ctx_t *ctx, char ch, int count) {
  for (int i = 0; i < count; i++) {
    out_char(ctx, ch);
  }
}

static int utoa_rev(unsigned long long val, unsigned base, char *buf) {
  int len = 0;
  do {
    unsigned digit = (unsigned)(val % base);
    buf[len++] = (digit < 10) ? (char)('0' + digit) : (char)('a' + digit - 10);
    val /= base;
  } while (val != 0);
  return len;
}

static void write_unsigned(out_ctx_t *ctx, unsigned long long val, unsigned base,
                           int width, char pad, const char *prefix, int prefix_len) {
  char tmp[32];
  int len = utoa_rev(val, base, tmp);
  int total = len + prefix_len;
  int pad_count = (width > total) ? (width - total) : 0;

  if (pad == '0') {
    for (int i = 0; i < prefix_len; i++) {
      out_char(ctx, prefix[i]);
    }
    out_repeat(ctx, '0', pad_count);
  } else {
    out_repeat(ctx, ' ', pad_count);
    for (int i = 0; i < prefix_len; i++) {
      out_char(ctx, prefix[i]);
    }
  }

  for (int i = len - 1; i >= 0; i--) {
    out_char(ctx, tmp[i]);
  }
}

static void write_signed(out_ctx_t *ctx, long long val, int width, char pad) {
  unsigned long long mag;
  char sign = 0;
  if (val < 0) {
    sign = '-';
    mag = (unsigned long long)(-(val + 1)) + 1;
  } else {
    mag = (unsigned long long)val;
  }
  if (sign) {
    write_unsigned(ctx, mag, 10, width, pad, &sign, 1);
  } else {
    write_unsigned(ctx, mag, 10, width, pad, NULL, 0);
  }
}

static int vformat(out_ctx_t *ctx, const char *fmt, va_list ap) {
  for (const char *p = fmt; *p != '\0'; p++) {
    if (*p != '%') {
      out_char(ctx, *p);
      continue;
    }

    p++;
    if (*p == '%') {
      out_char(ctx, '%');
      continue;
    }

    char pad = ' ';
    if (*p == '0') {
      pad = '0';
      p++;
    }

    int width = 0;
    while (*p >= '0' && *p <= '9') {
      width = width * 10 + (*p - '0');
      p++;
    }

    int len_mod = 0;
    if (*p == 'l') {
      p++;
      len_mod = 1;
      if (*p == 'l') {
        p++;
        len_mod = 2;
      }
    }

    switch (*p) {
      case 'c': {
        int ch = va_arg(ap, int);
        out_char(ctx, (char)ch);
        break;
      }
      case 's': {
        const char *s = va_arg(ap, const char *);
        if (s == NULL) s = "(null)";
        int len = 0;
        for (const char *q = s; *q; q++) len++;
        if (width > len) {
          out_repeat(ctx, ' ', width - len);
        }
        for (; *s; s++) {
          out_char(ctx, *s);
        }
        break;
      }
      case 'd':
      case 'i': {
        long long val = 0;
        if (len_mod == 2) val = va_arg(ap, long long);
        else if (len_mod == 1) val = va_arg(ap, long);
        else val = va_arg(ap, int);
        write_signed(ctx, val, width, pad);
        break;
      }
      case 'u': {
        unsigned long long val = 0;
        if (len_mod == 2) val = va_arg(ap, unsigned long long);
        else if (len_mod == 1) val = va_arg(ap, unsigned long);
        else val = va_arg(ap, unsigned int);
        write_unsigned(ctx, val, 10, width, pad, NULL, 0);
        break;
      }
      case 'x': {
        unsigned long long val = 0;
        if (len_mod == 2) val = va_arg(ap, unsigned long long);
        else if (len_mod == 1) val = va_arg(ap, unsigned long);
        else val = va_arg(ap, unsigned int);
        write_unsigned(ctx, val, 16, width, pad, NULL, 0);
        break;
      }
      case 'p': {
        uintptr_t val = (uintptr_t)va_arg(ap, void *);
        const char *prefix = "0x";
        char tmp[32];
        int len = utoa_rev((unsigned long long)val, 16, tmp);
        int digits = (int)(sizeof(void *) * 2);
        out_char(ctx, prefix[0]);
        out_char(ctx, prefix[1]);
        out_repeat(ctx, '0', (digits > len) ? (digits - len) : 0);
        for (int i = len - 1; i >= 0; i--) {
          out_char(ctx, tmp[i]);
        }
        break;
      }
      default:
        out_char(ctx, '%');
        out_char(ctx, *p);
        break;
    }
  }

  if (ctx->buf != NULL && ctx->size > 0) {
    size_t term = (ctx->pos < ctx->size) ? ctx->pos : (ctx->size - 1);
    ctx->buf[term] = '\0';
  }

  if (ctx->pos > (size_t)__INT_MAX__) {
    return __INT_MAX__;
  }
  return (int)ctx->pos;
}

int printf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  out_ctx_t ctx = { .buf = NULL, .size = 0, .pos = 0, .to_console = 1 };
  int result = vformat(&ctx, fmt, args);
  va_end(args);
  return result;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  return vsnprintf(out, __INT_MAX__, fmt, ap);
}

int sprintf(char *out, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int result = vsprintf(out, fmt, args);
  va_end(args);
  return result;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int result = vsnprintf(out, n, fmt, args);
  va_end(args);
  return result;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  if (n == 0)
    return 0;
  if (n > __INT_MAX__)
    n = __INT_MAX__;
  out_ctx_t ctx = { .buf = out, .size = n, .pos = 0, .to_console = 0 };
  return vformat(&ctx, fmt, ap);
}

#endif
