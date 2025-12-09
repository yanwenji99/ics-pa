#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint
{
    int NO;

    /* TODO: Add more members if necessary */
    
    int ival;
    char name[32];
    bool used;

    struct watchpoint *next;
} WP;

void init_wp_pool();
WP *new_wp();
void free_wp(WP *wp);
WP* find_wp(int no);
WP *find_head();
void show_wp();
WP *trigger_wp();