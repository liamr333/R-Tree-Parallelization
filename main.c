#include "r_tree.h"
#include "choose_leaf.h"
#include "pick_seeds.h"

// Slow and not recommended for large trees
#define SAVE_TO_CSV true
#define PRINT_TREE false
#define PRINT_TREE_SPECS true
#define NUM_CORES 12

struct timespec ts_begin, ts_end;
double elapsed;


int main(int argc, char *argv[]) {

	if (argc != 3) {
		fprintf(stderr, "Please provide 2 args: max_children (children per node), num_levels (number of levels in tree)\n");
		exit(1);
	}

	// Initialize randomness
	srand(time(NULL));

	int i, j, k;
	int index_records_per_node = atoi(argv[1]);
	// TO DO: Account for this 1-off thing (works for now but messy fix)
	int num_levels = atoi(argv[2]) - 1;

	if (num_levels < 0) {
		fprintf(stderr, "Levels (third argument) should be greater than or equal to 1\n");
		exit(1);
	}



	struct timespec start;
	struct timespec end;


	for (i = 1; i < NUM_CORES + 1; i++) {

		int num_insertions = 100;
		int num_threads = i;

		fprintf(stderr, "Doing %d insertions for %d threads\n", num_insertions, num_threads);

		double summed_time = 0;
		int num_rounds = 25;


		for (j = 0; j < num_rounds; j++) {

			r_tree_node *root = initialize_rt(index_records_per_node);
			generate_random_tree(root, num_levels, 0);

			MBR **insertion_mbrs = (MBR**)malloc(sizeof(MBR*) * num_insertions);
			index_record **insertion_irs = (index_record**)malloc(sizeof(index_record*) * num_insertions);

			// Initialize random insertion_records for insertion into R Tree
			for (k = 0; k < num_insertions; k++) {
				insertion_mbrs[k] = random_small_mbr(0, 0, MAX_RAND_NUM, MAX_RAND_NUM);
				insertion_irs[k] = initialize_ir(insertion_mbrs[k]);
			}


			clock_gettime(CLOCK_MONOTONIC, &start);

			for (k = 0; k < num_insertions; k++) {

				insert(&root, insertion_irs[k], num_threads);

			}

			clock_gettime(CLOCK_MONOTONIC, &end);

			free_tree(root);
			free(insertion_mbrs);
			free(insertion_irs);

			double duration = end.tv_sec - start.tv_sec;
			duration += (end.tv_nsec - start.tv_nsec) / 1000000000.0;

			summed_time += duration;

		}

		double average_duration = summed_time / num_rounds;

		fprintf(stderr, "Took an average of %lf seconds %d insertions in a tree with M=%d, levels=%d, and %d threads\n", average_duration, num_insertions, index_records_per_node, num_levels, num_threads);

	}

	return 0;
}
