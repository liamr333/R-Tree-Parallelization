#include "r_tree.h"
#include "linear_split.h"


/* args:
* r: the r_tree_node being split
* ir1: an index_record pointing to an r_tree_node
* ir2: an index_record pointing to an r_tree_node
* ir1 and ir2 are meant to be inserted into the r_tree_node above r, while r->parent
* will be removed
*/

void linear_split_sequential(r_tree_node *rt, index_record *ir1, index_record *ir2) {

	int i;

	r_tree_node *r1 = ir1->child;
	r_tree_node *r2 = ir2->child;


	for (i = 0; i < rt->num_members; i++) {
		double enlargement_1 = get_area_increase(ir1->mbr, rt->index_records[i]->mbr);
		double enlargement_2 = get_area_increase(ir2->mbr, rt->index_records[i]->mbr);

		if (enlargement_1 < enlargement_2) {
			add_member(r1, rt->index_records[i]);
		} else {
			add_member(r2, rt->index_records[i]);
		}
	}

}


void *linear_split_subset(void *arg) {
	param2 *params = (param2*)arg;
	r_tree_node *rt = params->rt;
	index_record *ir_1 = params->ir_1;
	index_record *ir_2 = params->ir_2;
	int start_index = params->start_index;
	int end_index = params->start_index;

	int i;

	for (i = start_index; i < end_index; i++) {
		// We put this here because in the thread initialization process, the last thread may be
		// set up to search for index_records that are out of bounds
		if (i == rt->num_members)
			break;


		double enlargement_1 = get_area_increase(ir_1->mbr, rt->index_records[i]->mbr);
		double enlargement_2 = get_area_increase(ir_2->mbr, rt->index_records[i]->mbr);

		if (enlargement_1 < enlargement_2) {
			split_nodes[i] = 0;
		} else {
			split_nodes[i] = 1;
		}
	}

	pthread_exit((void*)arg);
}


void linear_split_parallel(r_tree_node *rt, index_record *ir_1, index_record *ir_2, int num_threads) {

	int index_records_per_thread = (int) ceil((float)rt->num_members / num_threads);

	// Allocated shared list
	split_nodes = (int*)malloc(sizeof(int) * rt->num_members);

	param2 **param_objects = (param2**)malloc(sizeof(param2*) * num_threads);

	if (param_objects == NULL) {
		fprintf(stderr, "Malloc failed, exiting program\n");
		exit(1);
	}

	int i;

	for (i = 0; i < num_threads; i++) {
		param_objects[i] = (param2*)malloc(sizeof(param2));

		if (param_objects[i] == NULL) {
			fprintf(stderr, "Malloc failed, exiting program");
			exit(1);
		}

		param_objects[i]->start_index = i * index_records_per_thread;
		param_objects[i]->end_index = (i + 1) * index_records_per_thread;
		param_objects[i]->rt = rt;
		param_objects[i]->ir_1 = ir_1;
		param_objects[i]->ir_2 = ir_2;
	}


	for (i = 0; i < rt->num_members; i++) {
		if (split_nodes[i] == 0)
			add_member(ir_1->child, rt->index_records[i]);
		else
			add_member(ir_2->child, rt->index_records[i]);
	}


	// Free up shared list
	free(split_nodes);
}

