#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <trace.h>
#include <utils.h>

#include <elf.h>

#define RINGBUFFER_LEN 16

#ifdef CONFIG_ITRACE
typedef struct
{
  char logbuf[128];
  bool valid;
} TraceRingBufferEntry;

typedef struct
{
  TraceRingBufferEntry buf[RINGBUFFER_LEN];
  int next;
} TraceRingBuffer;

static TraceRingBuffer iringbuf = {};
#endif

#ifdef CONFIG_ISA64
typedef Elf64_Ehdr TraceElfEhdr;
typedef Elf64_Shdr TraceElfShdr;
typedef Elf64_Sym TraceElfSym;
#define TRACE_ELF_CLASS ELFCLASS64
#define TRACE_ST_TYPE ELF64_ST_TYPE
#else
typedef Elf32_Ehdr TraceElfEhdr;
typedef Elf32_Shdr TraceElfShdr;
typedef Elf32_Sym TraceElfSym;
#define TRACE_ELF_CLASS ELFCLASS32
#define TRACE_ST_TYPE ELF32_ST_TYPE
#endif

typedef struct
{
  vaddr_t start;
  vaddr_t end;
  char name[128];
} FuncSymbol;

/**
 * 静态全局变量
 * - func_symbols: 动态数组，存储所有函数符号
 * - nr_func_symbols: 当前已加载的函数数量
 * - elf_info: ELF文件的基本信息（入口点、段范围等）
 */
static FuncSymbol *func_symbols = NULL;
static int nr_func_symbols = 0;
static ElfLoadResult elf_info = {};
static int ftrace_depth = 0;

#define FTRACE_MAX_DEPTH 1024
typedef struct
{
  vaddr_t ret_addr;
  char name[128];
} FtraceFrame;

static FtraceFrame ftrace_stack[FTRACE_MAX_DEPTH];
static int ftrace_top = 0;

static void reset_ftrace_state(void)
{
  free(func_symbols);
  func_symbols = NULL;
  nr_func_symbols = 0;
  elf_info = (ElfLoadResult){};
}

/**
 * 从文件指定位置读取数据，失败则终止程序
 * @param fp    已打开的文件指针
 * @param offset 文件中的偏移量
 * @param buf   目标缓冲区
 * @param size  要读取的字节数
 * @param what  描述信息（用于错误提示）
 */
static void read_file_or_panic(FILE *fp, long offset, void *buf, size_t size, const char *what) // 从文件fp的offset位置读取size字节到buf中，如果失败则打印what并退出
{
  int ret = fseek(fp, offset, SEEK_SET);
  Assert(ret == 0, "Failed to seek %s", what);
  ret = fread(buf, 1, size, fp);
  Assert((size_t)ret == size, "Failed to read %s", what);
}

static void check_elf_header(const TraceElfEhdr *ehdr, const char *elf_file) // 检查ELF文件头是否合法，包括魔数和ELF类，如果不合法则打印错误信息并退出
{
  Assert(ehdr->e_ident[EI_MAG0] == ELFMAG0 &&
             ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
             ehdr->e_ident[EI_MAG2] == ELFMAG2 &&
             ehdr->e_ident[EI_MAG3] == ELFMAG3,
         "%s is not a valid ELF file", elf_file);
  Assert(ehdr->e_ident[EI_CLASS] == TRACE_ELF_CLASS,
         "%s ELF class does not match current ISA", elf_file);
}

/**
 * 加载节头表(Section Headers)
 * @param fp       已打开的ELF文件指针
 * @param ehdr     ELF文件头
 * @param elf_file ELF文件名
 * @return 动态分配的节头表数组，需要调用者free
 */
static TraceElfShdr *load_section_headers(FILE *fp, const TraceElfEhdr *ehdr, const char *elf_file) // 从ELF文件fp中读取ehdr指定的节头表到内存中，如果失败则打印错误信息并退出
{
  size_t shdr_size = ehdr->e_shentsize * ehdr->e_shnum;
  TraceElfShdr *shdr = malloc(shdr_size);
  Assert(shdr != NULL, "No memory for section headers of %s", elf_file);
  read_file_or_panic(fp, (long)ehdr->e_shoff, shdr, shdr_size, "section headers");
  return shdr;
}

/**
 * 加载字符串表(String Table)
 * @param fp    已打开的ELF文件指针
 * @param shdr  描述字符串表的节头
 * @param what  描述信息（用于错误提示）
 * @return 动态分配的字符串表（以'\0'结尾），需要调用者free
 */
static char *load_string_table(FILE *fp, const TraceElfShdr *shdr, const char *what) // 从ELF文件fp中读取shdr指定的字符串表到内存中，如果失败则打印错误信息并退出
{
  char *strtab = malloc(shdr->sh_size + 1);
  Assert(strtab != NULL, "No memory for %s", what);
  read_file_or_panic(fp, (long)shdr->sh_offset, strtab, shdr->sh_size, what);
  strtab[shdr->sh_size] = '\0';
  return strtab;
}

void init_ftrace(const char *elf_file)
{
  if (elf_file == NULL)
  {
    reset_ftrace_state();
    Log("ftrace: ELF file is not provided");
    return;
  }

  reset_ftrace_state();
  ftrace_depth = 0;
  ftrace_top = 0;
  elf_info = load_elf(elf_file);
  int nr_symbols = load_elf_symbols(elf_file);

  Log("ftrace: entry=" FMT_WORD " .text=[" FMT_WORD ", " FMT_WORD ") .data=[" FMT_WORD ", " FMT_WORD ") funcs=%d",
      elf_info.entry_point,
      elf_info.text_start, elf_info.text_end,
      elf_info.data_start, elf_info.data_end,
      nr_symbols);
}
/**
 * 加载ELF文件的基本信息（入口点、代码段和数据段范围）
 * @param elf_file ELF文件名
 * @return ELF基本信息结构体
 */
ElfLoadResult load_elf(const char *elf_file)
{
  ElfLoadResult result = {};
  if (elf_file == NULL)
  {
    return result;
  }

  FILE *fp = fopen(elf_file, "rb");
  Assert(fp != NULL, "Can not open ELF file '%s'", elf_file);

  // 1. 读取并验证ELF头
  TraceElfEhdr ehdr;
  read_file_or_panic(fp, 0, &ehdr, sizeof(ehdr), "ELF header");
  check_elf_header(&ehdr, elf_file);

  // 2. 记录入口点
  result.entry_point = (vaddr_t)ehdr.e_entry;

  // 3. 加载节头表
  TraceElfShdr *shdr = load_section_headers(fp, &ehdr, elf_file);

  // 4. 如果有节名字符串表，查找.text和.data段
  if (ehdr.e_shstrndx != SHN_UNDEF)
  {
    Assert(ehdr.e_shstrndx < ehdr.e_shnum, "Invalid section name table index in %s", elf_file);
    // 加载节名字符串表
    char *shstrtab = load_string_table(fp, &shdr[ehdr.e_shstrndx], "section name string table");

    for (int i = 0; i < ehdr.e_shnum; i++)
    {
      const char *secname = shstrtab + shdr[i].sh_name;
      if (strcmp(secname, ".text") == 0)
      {
        result.text_start = (vaddr_t)shdr[i].sh_addr;
        result.text_end = (vaddr_t)(shdr[i].sh_addr + shdr[i].sh_size);
      }
      else if (strcmp(secname, ".data") == 0)
      {
        result.data_start = (vaddr_t)shdr[i].sh_addr;
        result.data_end = (vaddr_t)(shdr[i].sh_addr + shdr[i].sh_size);
      }
    }

    free(shstrtab); // 释放节名字符串表
  }

  free(shdr); // 释放节头表
  fclose(fp);
  return result;
}
/**
 * 加载ELF文件中的函数符号
 * @param elf_file ELF文件名
 * @return 加载的函数数量
 */
int load_elf_symbols(const char *elf_file)
{
  if (elf_file == NULL)
  {
    return 0;
  }

  FILE *fp = fopen(elf_file, "rb");
  Assert(fp != NULL, "Can not open ELF file '%s'", elf_file);

  TraceElfEhdr ehdr;
  read_file_or_panic(fp, 0, &ehdr, sizeof(ehdr), "ELF header");
  check_elf_header(&ehdr, elf_file);

  TraceElfShdr *shdr = load_section_headers(fp, &ehdr, elf_file);

  // 在加载新符号之前，先释放之前加载的符号信息
  free(func_symbols);
  func_symbols = NULL;
  nr_func_symbols = 0;

  // 遍历所有节头，寻找符号表节（SHT_SYMTAB或SHT_DYNSYM）
  for (int i = 0; i < ehdr.e_shnum; i++)
  {
    if (shdr[i].sh_type != SHT_SYMTAB && shdr[i].sh_type != SHT_DYNSYM)
    {
      continue;
    }

    Assert(shdr[i].sh_link < ehdr.e_shnum, "Invalid string table link in %s", elf_file);

    // 加载字符串表
    char *strtab = load_string_table(fp, &shdr[shdr[i].sh_link], "symbol string table");
    size_t sym_count = shdr[i].sh_size / sizeof(TraceElfSym);
    TraceElfSym *symtab = malloc(shdr[i].sh_size);
    Assert(symtab != NULL, "No memory for symbol table of %s", elf_file);
    read_file_or_panic(fp, (long)shdr[i].sh_offset, symtab, shdr[i].sh_size, "symbol table");

    for (size_t j = 0; j < sym_count; j++)
    {
      const TraceElfSym *sym = &symtab[j];
      if (TRACE_ST_TYPE(sym->st_info) != STT_FUNC || sym->st_name == 0)
      {
        continue;
      }

      const char *name = strtab + sym->st_name;
      if (name[0] == '\0')
      {
        continue;
      }
      // 扩展符号表数组
      FuncSymbol *new_symbols = realloc(func_symbols, (nr_func_symbols + 1) * sizeof(FuncSymbol));
      Assert(new_symbols != NULL, "No memory for function symbols of %s", elf_file);
      func_symbols = new_symbols;
      // 记录函数信息
      func_symbols[nr_func_symbols].start = (vaddr_t)sym->st_value;
      func_symbols[nr_func_symbols].end = (vaddr_t)(sym->st_value + (sym->st_size == 0 ? 1 : sym->st_size));
      snprintf(func_symbols[nr_func_symbols].name, sizeof(func_symbols[nr_func_symbols].name), "%s", name);
      nr_func_symbols++;
    }

    free(symtab);
    free(strtab);
  }

  free(shdr);
  fclose(fp);
  return nr_func_symbols;
}

static const FuncSymbol *find_func_by_addr(vaddr_t addr) // 根据地址查找对应的函数符号，如果找到则返回指向该符号的指针，否则返回NULL
{
  for (int i = 0; i < nr_func_symbols; i++)
  {
    if (addr >= func_symbols[i].start && addr < func_symbols[i].end)
    {
      return &func_symbols[i];
    }
  }
  return NULL;
}

void trace_log_func(const char *func_name, vaddr_t func_addr, bool is_entry) // 记录函数调用和返回日志，包括函数名称、地址和调用/返回标志
{
#ifdef CONFIG_TRACE
  int indent_depth = is_entry ? ftrace_depth : (ftrace_depth > 0 ? ftrace_depth - 1 : 0);
  int indent_len = indent_depth * 2;
  if (indent_len > 120)
  {
    indent_len = 120;
  }

  char indent[121];
  memset(indent, ' ', indent_len);
  indent[indent_len] = '\0';

  log_write("ftrace:%s%s " FMT_WORD " <%s>\n",
            indent,
            is_entry ? "call" : "ret ",
            func_addr,
            func_name);

  if (is_entry)
  {
    ftrace_depth++;
  }
  else if (ftrace_depth > 0)
  {
    ftrace_depth--;
  }
#else
  (void)func_name;
  (void)func_addr;
  (void)is_entry;
#endif
}

#ifdef CONFIG_ISA_riscv
static inline int32_t sign_extend_12(uint32_t imm)
{
  return ((int32_t)(imm << 20)) >> 20;
}
#endif

void trace_func_call_ret(Decode *s) // 分析指令s，判断是否为函数调用或返回指令，并记录相应的日志信息
{
#ifdef CONFIG_ISA_riscv
  uint32_t inst = s->isa.inst;
  uint32_t opcode = inst & 0x7f;
  uint32_t rd = (inst >> 7) & 0x1f;

  if (opcode == 0x6f)
  {
    if (rd == 1 || rd == 5)
    {
      const FuncSymbol *callee = find_func_by_addr(s->dnpc);
      const char *name = callee ? callee->name : "?";
      trace_log_func(name, s->dnpc, true);

      if (ftrace_top < FTRACE_MAX_DEPTH)
      {
        ftrace_stack[ftrace_top].ret_addr = s->snpc;
        snprintf(ftrace_stack[ftrace_top].name, sizeof(ftrace_stack[ftrace_top].name), "%s", name);
        ftrace_top++;
      }
    }
    return;
  }

  if (opcode == 0x67)
  {
    uint32_t funct3 = (inst >> 12) & 0x7;
    uint32_t rs1 = (inst >> 15) & 0x1f;
    int32_t imm = sign_extend_12(inst >> 20);

    if (funct3 != 0)
    {
      return;
    }

    if (rd == 1 || rd == 5)
    {
      const FuncSymbol *callee = find_func_by_addr(s->dnpc);
      const char *name = callee ? callee->name : "?";
      trace_log_func(name, s->dnpc, true);

      if (ftrace_top < FTRACE_MAX_DEPTH)
      {
        ftrace_stack[ftrace_top].ret_addr = s->snpc;
        snprintf(ftrace_stack[ftrace_top].name, sizeof(ftrace_stack[ftrace_top].name), "%s", name);
        ftrace_top++;
      }
      return;
    }

    if (rd == 0 && (rs1 == 1 || rs1 == 5) && imm == 0)
    {
      const FuncSymbol *caller = find_func_by_addr(s->dnpc);
      const char *caller_name = caller ? caller->name : "?";

      if (ftrace_top > 0)
      {
        ftrace_top--;
      }

      trace_log_func(caller_name, s->dnpc, false);
      return;
    }
  }
#else
  (void)s;
#endif
}

void trace_format_inst(Decode *s)
{ // 格式化指令为字符串，存储在s->logbuf中
#ifdef CONFIG_ITRACE
  char *p = s->logbuf;
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", s->pc);
  int ilen = s->snpc - s->pc;
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst;
#ifdef CONFIG_ISA_x86
  for (i = 0; i < ilen; i++)
  {
#else
  for (i = ilen - 1; i >= 0; i--)
  {
#endif
    p += snprintf(p, 4, " %02x", inst[i]);
  }
  int ilen_max = MUXDEF(CONFIG_ISA_x86, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0)
    space_len = 0;
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

void trace_write_inst(Decode *s)
{ // 将s->logbuf中的指令信息写入日志文件
#ifdef CONFIG_ITRACE_COND
  if (ITRACE_COND)
  {
    log_write("%s\n", s->logbuf);
  }
#else
  (void)s;
#endif
}

void trace_print_step(Decode *s, bool print_step)
{ // 如果print_step为true，则将s->logbuf中的指令信息输出到屏幕
  if (print_step)
  {
    IFDEF(CONFIG_ITRACE, puts(s->logbuf));
  }
}

void trace_ringbuf_push(Decode *s)
{ // 将s->logbuf中的指令信息存储到循环缓冲区中，以便后续打印
#ifdef CONFIG_ITRACE
  TraceRingBufferEntry *entry = &iringbuf.buf[iringbuf.next];
  entry->valid = true;
  snprintf(entry->logbuf, sizeof(entry->logbuf), "%s", s->logbuf);
  iringbuf.next = (iringbuf.next + 1) % RINGBUFFER_LEN;
#else
  (void)s;
#endif
}

void trace_ringbuf_print(void)
{ // 打印循环缓冲区中的指令信息
#ifdef CONFIG_ITRACE
  int error_index = (iringbuf.next - 1 + RINGBUFFER_LEN) % RINGBUFFER_LEN;
  for (int i = 0; i < RINGBUFFER_LEN; i++)
  {
    int index = (iringbuf.next + i) % RINGBUFFER_LEN;
    TraceRingBufferEntry *entry = &iringbuf.buf[index];
    if (!entry->valid)
    {
      continue;
    }
    printf("%s %s\n", index == error_index ? "-->" : "   ", entry->logbuf);
  }
#endif
}

void trace_log_mem(char type, paddr_t addr, int len, word_t data)
{ // 记录内存访问日志，包括访问类型、地址、长度和数据
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

void trace_log_device(const char *name, char type, paddr_t addr, int len, word_t data)
{ // 记录设备访问日志，包括设备名、访问类型、地址、长度和数据
#ifdef CONFIG_DTRACE
  char logbuf[160];
  char *p = logbuf;

  p += snprintf(p, sizeof(logbuf), "dtrace: pc=" FMT_WORD, cpu.pc);
  p += snprintf(p, logbuf + sizeof(logbuf) - p, " name=%s", name);
  p += snprintf(p, logbuf + sizeof(logbuf) - p, " addr=" FMT_PADDR, addr);
  p += snprintf(p, logbuf + sizeof(logbuf) - p, " len=%d", len);
  p += snprintf(p, logbuf + sizeof(logbuf) - p, " type=%c", type);
  snprintf(p, logbuf + sizeof(logbuf) - p, " data=" FMT_WORD, data);
  log_write("%s\n", logbuf);
#else
  (void)name;
  (void)type;
  (void)addr;
  (void)len;
  (void)data;
#endif
}