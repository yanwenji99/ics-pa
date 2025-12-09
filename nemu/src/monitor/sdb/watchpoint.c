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

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  int ival;
  char name[32];

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP* new_wp(){
  WP *new_wp = (WP *)malloc(sizeof(WP));
  new_wp->next = head;
  head = new_wp;
  if(free_==NULL){
    printf("No free watchpoint!\n");
    assert(0);
  }
  else {
    new_wp->NO = free_->NO;
    free_ = free_->next;
  }
  return new_wp;
}

void free_wp(WP *wp){
  WP *temp = head;
  if(head==wp){
    head = head->next;
  }
  else{
    while(temp->next!=wp){
      temp = temp->next;
    }
    temp->next = wp->next;
  }
  wp->next = free_;;
  free_ = wp;
  free(wp);
}

WP* find_wp(int no){
  WP *temp = head;
  while(temp!=NULL){
    if(temp->NO==no){
      return temp;
    }
    temp = temp->next;
  }
  printf("Watchpoint %d not found!\n",no);
  return NULL;
}

WP *find_head(){
  return head;
}

void show_wp(){
  WP *temp = head;
  printf("Num\tName\tValue\n");
  while(temp!=NULL){
    printf("%d\t%s\t%d\n",temp->NO,temp->name,temp->ival);
    temp = temp->next;
  }
}

WP *trigger_wp(){
  WP *temp=find_head();
  WP *frist=temp;
  bool triggered=false;
  while(temp!=NULL){
    bool success=true;
    uint32_t new_val=vaddr_expr(temp->name, &success);
    if(new_val!=temp->ival){
      temp->ival=new_val;
      triggered=true;
      if(strcmp(temp->name,frist->name)<0)
        frist = temp;
    }
    temp=temp->next;
  }
  if(triggered==false)
    return NULL;
  else
    return frist;
}