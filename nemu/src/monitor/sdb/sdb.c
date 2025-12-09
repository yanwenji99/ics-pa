/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "sdb.h"
#include <memory/vaddr.h>
#include "watchpoint.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char *rl_gets(){
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  set_nemu_state(NEMU_QUIT, 0, 0);
  return -1;
}

static int cmd_info(char *args) {
  switch (args[0]) {
    case 'r':
      isa_reg_display();
      break;
    case 'w':
      show_wp();
      break;
    default:
      printf("Unknown info command '%s'\n", args);
      break;
  }
  return 0;
}

static int cmd_x(char *args) {
  char *num_str = strtok(args, " ");
  char *addr = strtok(NULL, " ");
  int N = strtol(num_str, NULL, 10);
  bool success = true;
  word_t start_addr = expr(addr, &success);
  if(start_addr<CONFIG_MBASE){
    start_addr+=CONFIG_MBASE;
  }
  for (int i = 0; i < N; i++) 
  {
    uint32_t data=vaddr_read(start_addr + i*4, 4);
    printf("0x%08x: 0x%08x\n", start_addr + i*4, data);
  }
  return 0;
}

static int cmd_si(char *args) {
  int N = 1;
  if (args != NULL) {
    char *num=strtok(args, " ");
    N = strtol(num, NULL, 10);
  }
  cpu_exec(N);
  return 0;
}

static int cmd_p(char *args) {
  bool success = true;
  word_t result = expr(args, &success);
  if (success) {
    printf("%u\n", result);
  } 
  else {
    printf("Wrong expression\n");
  }
  return 0;
}

static int cmd_fp(char *args) {
  FILE *fp = fopen(args, "r");
  if (fp == NULL) {
    printf("Cannot open file '%s'\n", args);
    return 0;
  }
  char buf[256];
  while(fgets(buf, sizeof(buf), fp) != NULL) {
    char *fresult = strtok(buf, " ");
    char *expression = strtok(NULL, "\n");
    bool success = true;
    word_t result = expr(expression, &success);
    if (success)
    {
      if(result==atoi(fresult))
        printf("%u = %s\n", result,fresult);
      else
        printf("%u != %s\n", result,fresult);
    }
    else
    {
      printf("Wrong expression\n");
    }
  }
  fclose(fp);
  return 0;
}

static int cmd_w(char *args){
  char *e=strtok(args, " ");
  WP *newwp=new_wp();
  strcpy(newwp->name,e);
  bool success=true;
  uint32_t val=expr(e, &success);
  newwp->ival=val;
  return 0;
}

static int cmd_d(char *args){
  int no = atoi(strtok(args, " "));
  free_wp(find_wp(no));
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "info", "Display state information of the program", cmd_info },
  { "x", "Scan memory", cmd_x },
  { "si", "Step into instruction", cmd_si },
  { "p", "Evaluate expression", cmd_p },
  { "fp"," Evaluate expression in file", cmd_fp },
  { "w", "Set a watchpoint", cmd_w },
  { "d", "Delete a watchpoint", cmd_d },
  
  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif
    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
