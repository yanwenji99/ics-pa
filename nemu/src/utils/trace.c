#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <trace.h>
#include <utils.h>

#ifndef CONFIG_TARGET_AM
#include <elf.h>
#endif

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

#ifndef CONFIG_TARGET_AM

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

typedef struct {
  vaddr_t start;
  vaddr_t end;
  char name[128];
} FuncSymbol;

static FuncSymbol *func_symbols = NULL;
static int nr_func_symbols = 0;
static ElfLoadResult elf_info = {};

static void reset_ftrace_state(void) {
  free(func_symbols);
  func_symbols = NULL;
  nr_func_symbols = 0;
  elf_info = (ElfLoadResult){};
}

static void read_file_or_panic(FILE *fp, long offset, void *buf, size_t size, const char *what) {
  int ret = fseek(fp, offset, SEEK_SET);
  Assert(ret == 0, "Failed to seek %s", what);
  ret = fread(buf, 1, size, fp);
  Assert((size_t)ret == size, "Failed to read %s", what);
}

static void check_elf_header(const TraceElfEhdr *ehdr, const char *elf_file) {
  Assert(ehdr->e_ident[EI_MAG0] == ELFMAG0 &&
         ehdr->e_ident[EI_MAG1] == ELFMAG1 &&
         ehdr->e_ident[EI_MAG2] == ELFMAG2 &&
         ehdr->e_ident[EI_MAG3] == ELFMAG3,
         "%s is not a valid ELF file", elf_file);
  Assert(ehdr->e_ident[EI_CLASS] == TRACE_ELF_CLASS,
         "%s ELF class does not match current ISA", elf_file);
}

static TraceElfShdr *load_section_headers(FILE *fp, const TraceElfEhdr *ehdr, const char *elf_file) {
  size_t shdr_size = ehdr->e_shentsize * ehdr->e_shnum;
  TraceElfShdr *shdr = malloc(shdr_size);
  Assert(shdr != NULL, "No memory for section headers of %s", elf_file);
  read_file_or_panic(fp, (long)ehdr->e_shoff, shdr, shdr_size, "section headers");
  return shdr;
}

static char *load_string_table(FILE *fp, const TraceElfShdr *shdr, const char *what) {
  char *strtab = malloc(shdr->sh_size + 1);
  Assert(strtab != NULL, "No memory for %s", what);
  read_file_or_panic(fp, (long)shdr->sh_offset, strtab, shdr->sh_size, what);
  strtab[shdr->sh_size] = '\0';
  return strtab;
}

#endif

void init_ftrace(const char *elf_file) {
#ifndef CONFIG_TARGET_AM
  if (elf_file == NULL) {
    reset_ftrace_state();
    Log("ftrace: ELF file is not provided");
    return;
  }

  reset_ftrace_state();
  elf_info = load_elf(elf_file);
  int nr_symbols = load_elf_symbols(elf_file);
  int nr_strings = load_elf_strings(elf_file);

  Log("ftrace: entry=" FMT_WORD " .text=[" FMT_WORD ", " FMT_WORD ") .data=[" FMT_WORD ", " FMT_WORD ") funcs=%d strings=%d",
      elf_info.entry_point,
      elf_info.text_start, elf_info.text_end,
      elf_info.data_start, elf_info.data_end,
      nr_symbols, nr_strings);
#else
  (void)elf_file;
#endif
}

ElfLoadResult load_elf(const char *elf_file) {
  ElfLoadResult result = {};

#ifndef CONFIG_TARGET_AM
  if (elf_file == NULL) {
    return result;
  }

  FILE *fp = fopen(elf_file, "rb");
  Assert(fp != NULL, "Can not open ELF file '%s'", elf_file);

  TraceElfEhdr ehdr;
  read_file_or_panic(fp, 0, &ehdr, sizeof(ehdr), "ELF header");
  check_elf_header(&ehdr, elf_file);

  result.entry_point = (vaddr_t)ehdr.e_entry;

  TraceElfShdr *shdr = load_section_headers(fp, &ehdr, elf_file);
  if (ehdr.e_shstrndx != SHN_UNDEF) {
    Assert(ehdr.e_shstrndx < ehdr.e_shnum, "Invalid section name table index in %s", elf_file);
    char *shstrtab = load_string_table(fp, &shdr[ehdr.e_shstrndx], "section name string table");

    for (int i = 0; i < ehdr.e_shnum; i++) {
      const char *secname = shstrtab + shdr[i].sh_name;
      if (strcmp(secname, ".text") == 0) {
        result.text_start = (vaddr_t)shdr[i].sh_addr;
        result.text_end = (vaddr_t)(shdr[i].sh_addr + shdr[i].sh_size);
      }
      else if (strcmp(secname, ".data") == 0) {
        result.data_start = (vaddr_t)shdr[i].sh_addr;
        result.data_end = (vaddr_t)(shdr[i].sh_addr + shdr[i].sh_size);
      }
    }

    free(shstrtab);
  }

  free(shdr);
  fclose(fp);
#else
  (void)elf_file;
#endif

  return result;
}

int load_elf_symbols(const char *elf_file) {
#ifndef CONFIG_TARGET_AM
  if (elf_file == NULL) {
    return 0;
  }

  FILE *fp = fopen(elf_file, "rb");
  Assert(fp != NULL, "Can not open ELF file '%s'", elf_file);

  TraceElfEhdr ehdr;
  read_file_or_panic(fp, 0, &ehdr, sizeof(ehdr), "ELF header");
  check_elf_header(&ehdr, elf_file);

  TraceElfShdr *shdr = load_section_headers(fp, &ehdr, elf_file);
  free(func_symbols);
  func_symbols = NULL;
  nr_func_symbols = 0;

  for (int i = 0; i < ehdr.e_shnum; i++) {
    if (shdr[i].sh_type != SHT_SYMTAB && shdr[i].sh_type != SHT_DYNSYM) {
      continue;
    }

    Assert(shdr[i].sh_link < ehdr.e_shnum, "Invalid string table link in %s", elf_file);

    char *strtab = load_string_table(fp, &shdr[shdr[i].sh_link], "symbol string table");
    size_t sym_count = shdr[i].sh_size / sizeof(TraceElfSym);
    TraceElfSym *symtab = malloc(shdr[i].sh_size);
    Assert(symtab != NULL, "No memory for symbol table of %s", elf_file);
    read_file_or_panic(fp, (long)shdr[i].sh_offset, symtab, shdr[i].sh_size, "symbol table");

    for (size_t j = 0; j < sym_count; j++) {
      const TraceElfSym *sym = &symtab[j];
      if (TRACE_ST_TYPE(sym->st_info) != STT_FUNC || sym->st_name == 0) {
        continue;
      }

      const char *name = strtab + sym->st_name;
      if (name[0] == '\0') {
        continue;
      }

      FuncSymbol *new_symbols = realloc(func_symbols, (nr_func_symbols + 1) * sizeof(FuncSymbol));
      Assert(new_symbols != NULL, "No memory for function symbols of %s", elf_file);
      func_symbols = new_symbols;

      func_symbols[nr_func_symbols].start = (vaddr_t)sym->st_value;
      func_symbols[nr_func_symbols].end = (vaddr_t)(sym->st_value + (sym->st_size == 0 ? 1 : sym->st_size));
      strncpy(func_symbols[nr_func_symbols].name, name, sizeof(func_symbols[nr_func_symbols].name) - 1);
      func_symbols[nr_func_symbols].name[sizeof(func_symbols[nr_func_symbols].name) - 1] = '\0';
      nr_func_symbols++;
    }

    free(symtab);
    free(strtab);
  }

  free(shdr);
  fclose(fp);
  return nr_func_symbols;
#else
  (void)elf_file;
  return 0;
#endif
}

int load_elf_strings(const char *elf_file) {
#ifndef CONFIG_TARGET_AM
  if (elf_file == NULL) {
    return 0;
  }

  FILE *fp = fopen(elf_file, "rb");
  Assert(fp != NULL, "Can not open ELF file '%s'", elf_file);

  TraceElfEhdr ehdr;
  read_file_or_panic(fp, 0, &ehdr, sizeof(ehdr), "ELF header");
  check_elf_header(&ehdr, elf_file);

  TraceElfShdr *shdr = load_section_headers(fp, &ehdr, elf_file);
  int nr_strings = 0;

  for (int i = 0; i < ehdr.e_shnum; i++) {
    if (shdr[i].sh_type != SHT_STRTAB || i == ehdr.e_shstrndx) {
      continue;
    }

    char *strtab = load_string_table(fp, &shdr[i], "string table");
    for (size_t j = 1; j < shdr[i].sh_size; j++) {
      if (strtab[j] != '\0' && strtab[j - 1] == '\0') {
        nr_strings++;
      }
    }
    free(strtab);
  }

  free(shdr);
  fclose(fp);
  return nr_strings;
#else
  (void)elf_file;
  return 0;
#endif
}

void trace_format_inst(Decode *s) { // 格式化指令为字符串，存储在s->logbuf中
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

void trace_write_inst(Decode *s) { // 将s->logbuf中的指令信息写入日志文件
#ifdef CONFIG_ITRACE_COND
  if (ITRACE_COND) {
    log_write("%s\n", s->logbuf);
  }
#else
  (void)s;
#endif
}

void trace_print_step(Decode *s, bool print_step) { // 如果print_step为true，则将s->logbuf中的指令信息输出到屏幕
  if (print_step) {
    IFDEF(CONFIG_ITRACE, puts(s->logbuf));
  }
}

void trace_ringbuf_push(Decode *s) { // 将s->logbuf中的指令信息存储到循环缓冲区中，以便后续打印
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

void trace_ringbuf_print(void) { // 打印循环缓冲区中的指令信息
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

void trace_log_mem(char type, paddr_t addr, int len, word_t data) { // 记录内存访问日志，包括访问类型、地址、长度和数据
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