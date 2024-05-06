#ifndef _linear_split_h
#define _linear_split_h

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

// binary list where if split_nodes[i] = 0, then the index_record at rt->index_records[i] will go to ir_1
// during the split, and conversely, if split_nodes[i] = 1, then the index_record at rt->index_records[i]
// will go to ir_2 during the split
int *split_nodes;

typedef struct param2 {
	r_tree_node *rt;

	// rt is being split, so all of its index_records will go to either ir_1->child or ir_2->child
	index_record *ir_1;
	index_record *ir_2;

	// Start_index and end_index indicate what subset of index_records the thread will look through
	// to assign each index_record to one of the new r_tree_nodes
	int start_index;
	int end_index;
} param2;


/* args:
* r: the r_tree_node being split
* ir1: an index_record pointing to an r_tree_node
* ir2: an index_record pointing to an r_tree_node
* ir1 and ir2 are meant to be inserted into the r_tree_node above r, while r->parent
* will be removed
*/

void linear_split_sequential(r_tree_node *r, index_record *ir1, index_record *ir2);


void *linear_split_subset(void *arg);


void linear_split_parallel(r_tree_node *rt, index_record *ir_1, index_record *ir_2, int num_threads);


#endif
