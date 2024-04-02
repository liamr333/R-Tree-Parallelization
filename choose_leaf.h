#ifndef _choose_leaf_h
#define _choose_leaf_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>


double min_enlargements[NUM_CORES];
int min_enlargement_indices[NUM_CORES];


// Used to store the parameters to be passed to the function for each thread which finds the minimum enlargement of an MBR when inserting a 
// new record
typedef struct param0 {
	struct r_tree_node *rt;
	struct index_record *insertion_ir;

	// Denote the bounds of the subset of index_records for which each thread will search for a min_enlargement in 
	int start_index;
	int end_index;

	// Index into the static array min_enlargements in which to write the result
	int result_index;
} param0;


// Given an r_tree_node "rt" and an index_record "insertion_ir" to be inserted, find the index_record to descend upon
// Part of the overall choose_leaf algorithm
int sequential_get_insertion_index(r_tree_node *rt, index_record *insertion_ir);


// Same as above but used for a thread to choose from a subset of all the index records in a node
void *parallel_get_insertion_index(void *arg);


// Used to initialize shared memory objects for the threads when they need to get the insertion index for a new record
param0 **initialize_threads(r_tree_node *rt, index_record *insertion_ir, int num_threads);


// Find the optimal leaf for insertion. Programmed according to Antonin Guttman's instructions in his 1984 paper
// I decided to not account for ties due to the great unlikelihood of there being an exact tie
r_tree_node *choose_leaf_sequential(r_tree_node *node, index_record *new_record);


r_tree_node *choose_leaf_parallel(r_tree_node *node, index_record *new_record, int num_threads);


#endif
