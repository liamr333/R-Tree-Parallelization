#include "r_tree.h"
#include "choose_leaf.h"


// Given an r_tree_node "rt" and an index_record "insertion_ir" to be inserted, find the index_record to descend upon
// Part of the overall choose_leaf algorithm
int sequential_get_insertion_index(r_tree_node *rt, index_record *insertion_ir) {
	int i;
	double min_enlargement = get_area_increase(rt->index_records[0]->mbr, insertion_ir->mbr);
	int curr_index = 0;

	for (i = 0; i < rt->num_members; i++) {
		double enlargement = get_area_increase(rt->index_records[i]->mbr, insertion_ir->mbr);
		if (enlargement < min_enlargement) {
			min_enlargement = enlargement;
			curr_index = i;
		}
	}

	return curr_index;
}


// Same as above but used for a thread to choose from a subset of all the index records in a node
void *parallel_get_insertion_index(void *arg) {
	param0 *p0;
	p0 = (param0*)arg;
	r_tree_node *rt = p0->rt;
	index_record *insertion_ir = p0->insertion_ir;
	int start_index = p0->start_index;
	int end_index = p0->end_index;
	int result_index = p0->result_index;
	int i;
	double min_enlargement = get_area_increase(rt->index_records[0]->mbr, insertion_ir->mbr);
	int curr_index = start_index;

	for (i = start_index; i < end_index && i < rt->num_members; i++) {
		double enlargement = get_area_increase(rt->index_records[i]->mbr, insertion_ir->mbr);
		if (enlargement < min_enlargement) {
			min_enlargement = enlargement;
			curr_index = i;
		}
	}


	min_enlargements[result_index] = min_enlargement;
	min_enlargement_indices[result_index] = curr_index;
	pthread_exit((void *)arg);
}


// Used to initialize shared memory objects for the threads when they need to get the insertion index for a new record
param0 **initialize_threads(r_tree_node *rt, index_record *insertion_ir, int num_threads) {
	int index_records_per_thread = (int) ceil((float)rt->num_members / num_threads);

	param0 **param_objects = (param0**)malloc(sizeof(param0*) * num_threads);

	if (param_objects == NULL) {
		fprintf(stderr, "Malloc failed, exiting program");
		exit(1);
	}

	int i;

	// Reset static shared arrays
	for (i = 0; i < NUM_CORES; i++) {
		min_enlargements[i] = 0.0;
		min_enlargement_indices[i] = 0;
	}


	// Initialize parameter objects to be passed to each thread
	for (i = 0; i < num_threads; i++) {
		param_objects[i] = (param0*)malloc(sizeof(param0));
		if (param_objects[i] == NULL) {
			fprintf(stderr, "Malloc failed, exiting program");
			exit(1);
		}
		param_objects[i]->start_index = i * index_records_per_thread;
		param_objects[i]->end_index = (i + 1) * index_records_per_thread;
		param_objects[i]->rt = rt;
		param_objects[i]->insertion_ir = insertion_ir;
		param_objects[i]->result_index = i;
	}

	return param_objects;
}


// Find the optimal leaf for insertion. Programmed according to Antonin Guttman's instructions in his 1984 paper
// I decided to not account for ties due to the great unlikelihood of there being an exact tie
r_tree_node *choose_leaf_sequential(r_tree_node *node, index_record *new_record) {
	if (is_leaf(node)) {
		return node;
	} else {
		int optimal_ir_index = sequential_get_insertion_index(node, new_record);
		return choose_leaf_sequential(node->index_records[optimal_ir_index]->child, new_record);
	}
}


r_tree_node *choose_leaf_parallel(r_tree_node *node, index_record *new_record, int num_threads) {

	int i;
	double min_enlargement;
	pthread_t thread_ids[num_threads];

	if (is_leaf(node)) {
		return node;
	} else {
		param0 **param_objects = initialize_threads(node, new_record, num_threads);
		for (i = 0; i < num_threads; i++)
			pthread_create(&thread_ids[i], NULL, parallel_get_insertion_index, (void*)param_objects[i]);
		for (i = 0; i < num_threads; i++)
			pthread_join(thread_ids[i], NULL);

		// Get the minimum of the local minimum enlargement indices that the threads have written
		min_enlargement = min_enlargements[0];

		int curr_index = min_enlargement_indices[0];

		for (i = 0; i < num_threads; i++) {
			if (min_enlargements[i] < min_enlargement) {
				min_enlargement = min_enlargements[i];
				curr_index = min_enlargement_indices[i];
			}
		}

		int optimal_ir_index = curr_index;

		return choose_leaf_parallel(node->index_records[optimal_ir_index]->child, new_record, num_threads);
	}
}

