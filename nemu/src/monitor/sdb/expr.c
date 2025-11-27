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

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, TK_EQ,

  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"==", TK_EQ},        // equal
  {"-", '-'},          // minus
  {"\\*", '*'},         // multiply
  {"/", '/'},           // divide
  {"\\(", '('},         // left parenthesis
  {"\\)", ')'},         // right parenthesis
  {"[0-9]+", 'n'},      // number
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE:
            break;
          case '+':
            tokens[nr_token++].type = '+';
            break;
          case '-':
            tokens[nr_token++].type = '-';
            break;
          case '*':
            tokens[nr_token++].type = '*';
            break;
          case '/':
            tokens[nr_token++].type = '/';
            break;
          case '(':
            tokens[nr_token++].type = '(';
            break;
          case ')':
            tokens[nr_token++].type = ')';
            break;
          case TK_EQ:
            tokens[nr_token++].type = TK_EQ;
            break;
          case 'n':
            tokens[nr_token].type = 'n';
            break;
          default:
            //TODO();
            panic("Error: unrecognized token type");
          }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

bool check_parentheses(Token *tokens, int start, int end)
{
  if (tokens[start].type != '(' || tokens[end].type != ')')
  {
    return false;
  }

  int cnt = 0;
  for (int i = start; i <= end; i++)
  {
    if (tokens[i].type == '(')
      cnt++;
    else if (tokens[i].type == ')')
      cnt--;

    if (cnt == 0 && i != end)
    {
      return false;
    }
  }

  return cnt == 0;
}

static int dominant_op(Token *tokens,int start,int end)
{
  int main[end - start + 1], index = -1, i;
  for (i = start; i <= end; i++)
    main[i-start]=1;
  for(i=start;i<=end;i++)
  {
    if(tokens[i].type=='n')
      main[i-start]=0;
    else if(tokens[i].type=='(')
    {
      int cnt=1;  
      for(int j=i+1;j<=end;j++)
      {
        if(tokens[j].type=='(')
          cnt++;
        else if(tokens[j].type==')')
          cnt--;
        if(cnt==0)
        {
          for(int k=i;k<=j;k++)
            main[k-start]=0;
          i=j;
          break;
        }
      }
    }
  }
  for(i=end;i>=start;i--)
  {
    if(main[i-start]==1&&(tokens[i].type=='+'||tokens[i].type=='-'))
      index=i;
  }
  if(index==-1)
  {
    for(i=end;i>=start;i--)
    {
      if(main[i-start]==1&&(tokens[i].type=='*'||tokens[i].type=='/'))
        index=i;
    }
  }
  return index;
}

static word_t eval(Token *tokens,int p,int q) 
{
  if(p>q)
  {
    printf("Bad expression\n");
    return 0;
  }
  else if(p==q)
  {
    return atoi(tokens[p].str);
  }
  else if(check_parentheses(tokens,p,q)==true)
  {
    return eval(tokens,p+1,q-1);
  }
  else
  {
    int index=dominant_op(tokens,p,q);
    word_t val1 = eval(tokens, p, index);
    word_t val2 = eval(tokens, index + 1, q);
    switch (tokens[index].type) {
      case '+': return val1 + val2;
      case '-': return val1 - val2;
      case '*': return val1 * val2;
      case '/': return val1 / val2;
      default: assert(0);
    }
    return 0;
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO/eval: Insert codes to evaluate the expression. */
  int p=0,q=nr_token-1;
  word_t result = eval(tokens, p, q);

  return result;
}
