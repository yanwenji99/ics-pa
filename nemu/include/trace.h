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

// ftrace
void init_ftrace(const char *elf_file); // 初始化函数调用跟踪，加载ELF文件并准备符号信息
ElfLoadResult load_elf(const char *elf_file); // 加载ELF文件并返回相关信息
int load_elf_symbols(const char *filename); // 加载ELF文件中的符号表，返回符号数量
void trace_log_func(const char *func_name, vaddr_t func_addr, bool is_entry); // 记录函数调用和返回日志，包括函数名称、地址和调用/返回标志
void trace_func_call_ret(Decode *s);                                          // 分析指令s，判断是否为函数调用或返回指令，并记录相应的日志信息

// itrace
void trace_format_inst(Decode *s); // 格式化指令为字符串，存储在s->logbuf中
void trace_write_inst(Decode *s); // 将s->logbuf中的指令信息写入日志文件

// iringbuf
void trace_print_step(Decode *s, bool print_step); // 如果print_step为true，则将s->logbuf中的指令信息输出到屏幕
void trace_ringbuf_push(Decode *s); // 将s->logbuf中的指令信息存储到循环缓冲区中，以便后续打印
void trace_ringbuf_print(void); // 打印循环缓冲区中的指令信息

// mtrace
void trace_log_mem(char type, paddr_t addr, int len, word_t data); // 记录内存访问日志，包括访问类型、地址、长度和数据

// dtrace
void trace_log_device(const char *name, char type, paddr_t addr, int len, word_t data); // 记录设备访问日志，包括设备名、访问类型、地址、长度和数据


#endif
