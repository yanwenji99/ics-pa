#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <trace.h>
#include <utils.h>

#define RINGBUFFER_LEN 16

typedef struct {
  char logbuf[128];
  bool valid;
} TraceRingBufferEntry;

typedef struct {
  TraceRingBufferEntry buf[RINGBUFFER_LEN];
  int next;
} TraceRingBuffer;

static TraceRingBuffer iringbuf = {};

void init_trace(const char *elf_file) {
  (void)elf_file;
}

void trace_format_inst(Decode *s) {
#ifdef CONFIG_ITRACE
  char *p = s->logbuf;
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", s->pc);
  int ilen = s->snpc - s->pc;
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst;
#ifdef CONFIG_ISA_x86
  for (i = 0; i < ilen; i ++) {
#else
  for (i = ilen - 1; i >= 0; i --) {
#endif
    p += snprintf(p, 4, " %02x", inst[i]);
  }
  int ilen_max = MUXDEF(CONFIG_ISA_x86, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0) space_len = 0;
  space_len = space_len * 3 + 1;
  memset(p, ' ', space_len);
  p += space_len;

  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  disassemble(p, s->logbuf + sizeof(s->logbuf) - p,
      MUXDEF(CONFIG_ISA_x86, s->snpc, s->pc), (uint8_t *)&s->isa.inst, ilen);
#else
  (void)s;
#endif
}

void trace_write_inst(Decode *s) {
#ifdef CONFIG_ITRACE_COND
  if (ITRACE_COND) {
    log_write("%s\n", s->logbuf);
  }
#else
  (void)s;
#endif
}

void trace_print_step(Decode *s, bool print_step) {
  if (print_step) {
    IFDEF(CONFIG_ITRACE, puts(s->logbuf));
  }
}

void trace_ringbuf_push(Decode *s) {
#ifdef CONFIG_ITRACE
  TraceRingBufferEntry *entry = &iringbuf.buf[iringbuf.next];
  entry->valid = true;
  strncpy(entry->logbuf, s->logbuf, sizeof(entry->logbuf) - 1);
  entry->logbuf[sizeof(entry->logbuf) - 1] = '\0';
  iringbuf.next = (iringbuf.next + 1) % RINGBUFFER_LEN;
#else
  (void)s;
#endif
}

void trace_ringbuf_print(void) {
#ifdef CONFIG_ITRACE
  int error_index = (iringbuf.next - 1 + RINGBUFFER_LEN) % RINGBUFFER_LEN;
  for (int i = 0; i < RINGBUFFER_LEN; i++) {
    int index = (iringbuf.next + i) % RINGBUFFER_LEN;
    TraceRingBufferEntry *entry = &iringbuf.buf[index];
    if (!entry->valid) {
      continue;
    }
    printf("%s %s\n", index == error_index ? "-->" : "   ", entry->logbuf);
  }
#endif
}

void trace_log_mem(char type, paddr_t addr, int len, word_t data) {
#ifdef CONFIG_MTRACE
  char logbuf[128];
  char *p = logbuf;

  p += snprintf(p, sizeof(logbuf), "mtrace: pc=" FMT_WORD, cpu.pc);
  p += snprintf(p, logbuf + sizeof(logbuf) - p, " addr=" FMT_PADDR, addr);
  p += snprintf(p, logbuf + sizeof(logbuf) - p, " len=%d", len);
  p += snprintf(p, logbuf + sizeof(logbuf) - p, " type=%c", type);
  snprintf(p, logbuf + sizeof(logbuf) - p, " data=" FMT_WORD, data);
  log_write("%s\n", logbuf);
#else
  (void)type;
  (void)addr;
  (void)len;
  (void)data;
#endif
}