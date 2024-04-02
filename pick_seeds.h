#ifndef _pick_seeds_h
#define _pick_seeds_h

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

// To be written to by threads
double greatest_wastes[NUM_CORES];
int *seed_indices_list[NUM_CORES];


typedef struct param1 {
	r_tree_node *rt;
	int start_index;
	int end_index;
} param1;


int *pick_seeds_sequential(index_record **irs, int num_index_records, double *biggest_waste);

void *pick_seeds_subset(void *arg);

int *pick_seeds_parallel(r_tree_node *rt, index_record *insertion_ir, int num_threads);


#endif
