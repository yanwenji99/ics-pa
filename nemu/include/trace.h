#ifndef __TRACE_H__
#define __TRACE_H__

#include <common.h>

typedef struct Decode Decode;

void init_trace(const char *elf_file);
void trace_format_inst(Decode *s);
void trace_write_inst(Decode *s);
void trace_print_step(Decode *s, bool print_step);
void trace_ringbuf_push(Decode *s);
void trace_ringbuf_print(void);
void trace_log_mem(char type, paddr_t addr, int len, word_t data);

#endif
