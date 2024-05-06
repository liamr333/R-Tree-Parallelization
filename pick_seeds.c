#include "r_tree.h"
#include "pick_seeds.h"



int *pick_seeds_sequential(index_record **irs, int num_index_records, double *biggest_waste) {
	int *seed_indices = (int*)malloc(sizeof(int) * 2);
	seed_indices[0] = 0;
	seed_indices[1] = 1;


	double curr_biggest_waste = get_merged_area(irs[0]->mbr, irs[1]->mbr) - get_area(irs[0]->mbr) - get_area(irs[1]->mbr);

	int i;
	int j;

	for (i = 0; i < num_index_records; i++) {
		for (j = 0; j < num_index_records; j++) {
			double curr_waste = get_merged_area(irs[i]->mbr, irs[j]->mbr) - get_area(irs[i]->mbr) - get_area(irs[j]->mbr);
			if (curr_waste > curr_biggest_waste) {
				curr_biggest_waste = curr_waste;
				seed_indices[0] = i;
				seed_indices[1] = j;
			}
		}
	}

	return seed_indices;
}



void *pick_seeds_subset(void *arg) {

	param1 *p = (param1*)arg;

	r_tree_node *rt = p->rt;

	// Set greatest waste to the greatest waste of the first two rectangles
	// It will be updated as we find other pairs of rectangles with larger wastes
	int i, j;
	double first_mrb_area = get_area(rt->index_records[0]->mbr);
	double second_mrb_area = get_area(rt->index_records[1]->mbr);
	double curr_biggest_waste = get_merged_area(rt->index_records[0]->mbr, rt->index_records[1]->mbr) - first_mrb_area - second_mrb_area;
	int *seed_indices = (int*)malloc(sizeof(int) * 2);
	int thread_index = p->start_index / (p->end_index - p->start_index);

	seed_indices[0] = 0;
	seed_indices[1] = 1;

	for (i = p->start_index; i < p->end_index && i < rt->num_members; i++) {
		for (j = 0; j < rt->num_members; j++) {
			double rect1_area = get_area(rt->index_records[i]->mbr);
			double rect2_area = get_area(rt->index_records[j]->mbr);
			double curr_waste = get_merged_area(rt->index_records[i]->mbr, rt->index_records[j]->mbr) - rect1_area - rect2_area;
			if (curr_waste > curr_biggest_waste) {
				curr_biggest_waste = curr_waste;
				seed_indices[0] = i;
				seed_indices[1] = j;
			}
		}
	}

	seed_indices_list[thread_index] = seed_indices;
	greatest_wastes[thread_index] = curr_biggest_waste;


	pthread_exit((void*)arg);
}


int *pick_seeds_parallel(r_tree_node *rt, index_record *insertion_ir, int num_threads) {

	if (num_threads > NUM_CORES)
		num_threads = NUM_CORES;



	pthread_t threads[num_threads];

	int i;
	int num_index_records_per_thread = (int) ceil((float)(rt->num_members + 1) / num_threads);

	param1 **params = (param1**)malloc(sizeof(param1*) * num_threads);

	for (i = 0; i < num_threads; i++) {
		params[i] = (param1*)malloc(sizeof(param1));
		params[i]->rt = rt;
		params[i]->start_index = i * num_index_records_per_thread;
		params[i]->end_index = (int) fmin((double) rt->num_members, (double) ((i + 1) * num_index_records_per_thread));
	}


	for (i = 0; i < num_threads; i++) {
		pthread_create(&threads[i], NULL, pick_seeds_subset, (void*)params[i]);
	}

	for (i = 0; i < num_threads; i++) {
		pthread_join(threads[i], NULL);
	}


	double greatest_waste = greatest_wastes[0];
	int *seed_indices = seed_indices_list[0];


	for (i = 0; i < num_threads; i++) {
		if (greatest_wastes[i] > greatest_waste) {
			greatest_waste = greatest_wastes[i];
			seed_indices = seed_indices_list[i];
		}
	}


	// Reset shared memory resources
	for (i = 0; i < num_threads; i++) {
		greatest_wastes[i] = 0.0;
	}

	return seed_indices;
}
