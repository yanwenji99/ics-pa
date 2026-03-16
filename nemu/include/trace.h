#ifndef __TRACE_H__
#define __TRACE_H__

#include <common.h>

typedef struct Decode Decode;

typedef struct
{
    vaddr_t entry_point; // 程序入口点
    vaddr_t text_start;  // 代码段起始地址
    vaddr_t text_end;    // 代码段结束地址
    vaddr_t data_start;  // 数据段起始地址
    vaddr_t data_end;    // 数据段结束地址
} ElfLoadResult;

void init_ftrace(const char *elf_file);
ElfLoadResult load_elf(const char *elf_file); // 加载ELF文件并返回相关信息
int load_elf_symbols(const char *filename);   // 加载ELF文件中的符号表，返回符号数量
void trace_format_inst(Decode *s);
void trace_write_inst(Decode *s);
void trace_print_step(Decode *s, bool print_step);
void trace_ringbuf_push(Decode *s);
void trace_ringbuf_print(void);
void trace_log_mem(char type, paddr_t addr, int len, word_t data);
void trace_log_func(const char *func_name, vaddr_t func_addr, bool is_entry);
void trace_func_call_ret(Decode *s);

#endif
