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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";
static int div_zero = 0;
static int deep = 0;

static void gen_rand_expr() {
  deep++;
  if(deep>10){
    deep--;
    int num;
    if (div_zero)
    {
      num = rand()%99+1;
      div_zero = 0;
    }
    else {
      num = rand()%100;
    }
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%d", num);
    return;
  }
  int quanzhong = rand() % 10;
  int choose;
  if(quanzhong<3)
    choose=0;
  else if(quanzhong<6)
    choose=1;
  else
    choose=2;
  switch(choose){
    case 0:
      int num;
      if (div_zero)
      {
        num = rand()%99+1;
        div_zero = 0;
      }
      else {
        num = rand()%100;
      }
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%d", num);
      deep--;
      break;
    case 1:
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "( ");
      gen_rand_expr();
      deep--;
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " )");
      break;
    case 2:
      gen_rand_expr();
      int op = rand()%4;
      switch(op){
        case 0:
          snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " + ");
          break;
        case 1:   
          snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " - ");
          break;
        case 2:
          snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " * ");
          break;
        case 3:
          snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " / ");
          div_zero = 1;
          break;
      }
      gen_rand_expr();
      deep--;
      break;
  }
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    buf[0] = '\0';
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
