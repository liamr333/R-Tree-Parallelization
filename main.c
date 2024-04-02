#include "r_tree.h"
#include "choose_leaf.h"
#include "pick_seeds.h"

// Slow and not recommended for large trees
#define SAVE_TO_CSV false
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

	int i;
	int index_records_per_node = atoi(argv[1]);
	// TO DO: Account for this 1-off thing (works for now but messy fix)
	int num_levels = atoi(argv[2]) - 1;

	if (num_levels < 0) {
		fprintf(stderr, "Levels (third argument) should be greater than or equal to 1\n");
		exit(1);
	}

	r_tree_node *root = initialize_rt(index_records_per_node);

	generate_random_tree(root, num_levels, 0);

	if (PRINT_TREE)
		print_tree(root, 0);

	if (PRINT_TREE_SPECS)
		print_tree_specs();

	// Generate an index record with a small MBR right in the center for testing out insertion time
	MBR *insertion_mbr = create_mbr(48.5, 48.9, 50.5, 50.5);
	index_record *insertion_ir = initialize_ir(insertion_mbr);
	r_tree_node *insertion_rt_node_1;
	r_tree_node *insertion_rt_node_2;

	FILE *f;

	if (access("output_times.csv", F_OK) == 0) {
		f = fopen("output_times.csv", "a");
	} else {
		f = fopen("output_times.csv", "w+");

		char *col1 = "index_records_per_node";
		char *col2 = "num_levels";
		char *col3 = "num_threads";
		char *col4 = "average_time_elapsed";

		fprintf(f, "%s,%s,%s,%s\n", col1, col2, col3, col4);
	}

	// Going up to 12 threads because the fox servers have 12 cores
	for (i = 1; i < NUM_CORES + 1; i++) {

		int j;
		int num_threads = i;

		if (num_threads > index_records_per_node) {
			fprintf(stderr, "Stopping here. More threads than nodes\n");
			exit(1);
		}


		double sum_elapsed = 0;

		// For each number of threads, get the average of 25 tests
		for (j = 0; j < 25; j++) {

			if (num_threads == 1) {
				clock_gettime(CLOCK_MONOTONIC, &ts_begin);
        			insertion_rt_node_1 = choose_leaf_sequential(root, insertion_ir);
				double bw;
				int *seed_indices = pick_seeds_sequential(root->index_records, root->num_members, &bw);
        			clock_gettime(CLOCK_MONOTONIC, &ts_end);

        			elapsed = ts_end.tv_sec - ts_begin.tv_sec;
        			elapsed += (ts_end.tv_nsec - ts_begin.tv_nsec) / 1000000000.0;

        			printf("Sequential algorithm found node for insertion: N%d\n", insertion_rt_node_1->index);
        			printf("Sequential algorithm found two seeds for splitting: N%d, N%d\n", seed_indices[0], seed_indices[1]);
				printf("Time taken: %lf seconds\n", elapsed);

			} else {

	        		clock_gettime(CLOCK_MONOTONIC, &ts_begin);
       	 			insertion_rt_node_2 = choose_leaf_parallel(root, insertion_ir, num_threads);
       		 		int *seed_indices = pick_seeds_parallel(root, insertion_ir, num_threads);
				clock_gettime(CLOCK_MONOTONIC, &ts_end);

        			elapsed = ts_end.tv_sec - ts_begin.tv_sec;
        			elapsed += (ts_end.tv_nsec - ts_begin.tv_nsec) / 1000000000.0;
        			printf("Parallel algorithm with %d threads found node for insertion: N%d\n", num_threads, insertion_rt_node_2->index);
				printf("Parallel algorithm with %d threads found two seeds for splitting: N%d, N%d\n", num_threads, seed_indices[0], seed_indices[1]);
        			printf("Time taken: %lf seconds\n", elapsed);
			}

			sum_elapsed = sum_elapsed + elapsed;

		}

		double average_elapsed = sum_elapsed / 25;
		fprintf(f, "%d,%d,%d,%lf\n", index_records_per_node, num_levels + 1, num_threads, average_elapsed);

	}

	fclose(f);


	if (SAVE_TO_CSV) {
		printf("Writing tree to csv (this may take a while)\n");
		write_tree_to_csv("output.csv", root);
	}

	free_tree(root);
	free(insertion_mbr);
	free(insertion_ir);

	return 0;
}
