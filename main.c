#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include <float.h>
#include <string.h>
#include <unistd.h>

// Trees cannot exceed 512MB in total size
#define MAX_TREE_SIZE 4294967296


// Percentage of index_records in r_tree_node's as opposed to empty slots when randomly generating r-trees
#define TREE_DENSITY 1
#define MAX_RAND_NUM 100

// Slow and not recommended for large trees
#define SAVE_TO_CSV false
#define PRINT_TREE false
#define PRINT_TREE_SPECS true
#define NUM_CORES 12

static int next_node_index = 0;
static long current_tree_size = 0;
static long num_tree_nodes = 0;
//static double *min_enlargements = NULL;
//static int *min_enlargement_indices = NULL;
static double min_enlargements[NUM_CORES];
static int min_enlargement_indices[NUM_CORES];

struct timespec ts_begin, ts_end;
double elapsed;

typedef struct MBR {
	double min_x;
	double min_y;
	double max_x;
	double max_y;
} MBR;


typedef struct index_record {
	struct MBR *mbr;
	struct r_tree_node *child;
} index_record;


typedef struct r_tree_node {
	int max_members;
	int num_members;
	int index;
	struct index_record *parent;
	struct index_record **index_records;
} r_tree_node;

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



// Generate a random double value that is less than min and greater than max
// Used to make randomly generated R-trees valid
double random_within_range(double min, double max) {
    	double range = max - min;
	double rand_val = min + ((double)rand() / RAND_MAX) * range;
	
	return rand_val;
}


double get_area(MBR *mbr) {
        return (mbr->max_x - mbr->min_x) * (mbr->max_y - mbr->min_y);
}


// Create a minimum bounding rectangle by providing a lower left coordinate (min_x, min_y)
// and an upper-right hand coordinate (max_x, max_y)

MBR *create_mbr(int min_x, int min_y, int max_x, int max_y) {

        MBR *new_mbr = (MBR *)malloc(sizeof(MBR));

        if (new_mbr == NULL)
                exit(1);

        new_mbr->min_x = min_x;
        new_mbr->min_y = min_y;
        new_mbr->max_x = max_x;
        new_mbr->max_y = max_y;


        return new_mbr;
}


// Generate a minimum bounding rectangle with each coordinate
// min_min_x, min_min_y, max_max_x, and max_max_y provide the bounds within which the randomly generated MBR
// can be generated. Otherwise, it may be too big and not fit within its parent
MBR *random_mbr(double min_min_x, double min_min_y, double max_max_x, double max_max_y) {
        MBR *new_mbr = (MBR *)malloc(sizeof(MBR));

	current_tree_size  = current_tree_size + sizeof(MBR);

	if (current_tree_size > MAX_TREE_SIZE) {
		fprintf(stderr, "Max tree size exceeded. Exiting program\n");
		exit(1);
	}

        if (new_mbr == NULL)
                exit(1);


        double x1 = random_within_range(min_min_x, max_max_x);
	double x2 = random_within_range(min_min_x, max_max_x);
	double y1 = random_within_range(min_min_y, max_max_y);
	double y2 = random_within_range(min_min_y, max_max_y);


        new_mbr->min_x = fmin(x1, x2);
        new_mbr->min_y = fmin(y1, y2);
        new_mbr->max_x = fmax(x1, x2);
        new_mbr->max_y = fmax(y1, y2);


        return new_mbr;
}



// Test and see what the area increase to an MBR would be if you add another child to it

double get_area_increase(MBR *original, MBR *new_child) {
        // Added small time delay to this function so that the effect of the parallelism with relation to context switching is more obvious

	double original_area = get_area(original);


        if (original_area == 0)
                return DBL_MAX;


        double new_width = fmax(original->max_x, new_child->max_x) - fmin(original->min_x, new_child->min_x);
        double new_height = fmax(original->max_y, new_child->max_y) - fmin(original->min_y, new_child->min_y);


        double new_area = new_width * new_height;

	usleep(100);
        return new_area - original_area;
}


index_record *initialize_ir(MBR *mbr) {
	index_record *ir = (index_record *)malloc(sizeof(index_record));

	current_tree_size = current_tree_size + sizeof(index_record);

	if (current_tree_size > MAX_TREE_SIZE) {
		fprintf(stderr, "Max tree size exceeded. Exiting program\n");
		exit(1);
	}

	if (ir == NULL) {
		fprintf(stderr, "Malloc failed in initialize_ir(). Exiting program\n");
		exit(1);
	}

	ir->mbr = mbr;
	ir->child = NULL;
	return ir;
}


r_tree_node *initialize_rt(int max_members) {
	r_tree_node *rt = (r_tree_node *)malloc(sizeof(r_tree_node));
	
	current_tree_size = current_tree_size + sizeof(r_tree_node);
	num_tree_nodes = num_tree_nodes + 1;
	
	if (current_tree_size > MAX_TREE_SIZE) {
		fprintf(stderr, "Max tree size exceeded. Exiting program\n");
		exit(1);
	}
	
	if (rt == NULL) {
		fprintf(stderr, "Malloc failed in initialize_rt(). Exiting program\n");
		exit(1);
	}

	rt->max_members = max_members;
	rt->num_members = 0;
	rt->index_records = (index_record**)malloc(sizeof(index_record *) * max_members);
	rt->index = next_node_index;
	next_node_index++;

	if (rt->index_records == NULL) {
		fprintf(stderr, "Malloc failed in initialize_rt(). Exiting program\n");
		exit(1);
	}

	rt->parent = NULL;
	return rt;
}


// Returns true if successfully added a new index record to the r_tree_node, false if you have reached max capacity and need to split
bool add_member(r_tree_node *host_node, index_record *new_member) {
	if (host_node->num_members == host_node->max_members)
		return false;
	host_node->index_records[host_node->num_members] = new_member;
	host_node->num_members++;
	return true;
}

// Returns true if the r_tree_node is a bottom-level node
// You can see that this is true when the node's index_record's point to NULL
bool is_leaf(r_tree_node *node) {
	return node->index_records[0]->child == NULL;
}


// Generates a random r-tree where each r_tree_node has r_tree_node->max_members / 2 entries
// Pass current_level as 0
void generate_random_tree(r_tree_node *node, int max_levels, int current_level) {
	int i;

	// If you are at a leaf node
	if (current_level >= max_levels) {
	
		for (i = 0; i < node->max_members * TREE_DENSITY; i++) {

			MBR *rand_mbr;
			
			if (node->parent != NULL)
				rand_mbr = random_mbr(node->parent->mbr->min_x, node->parent->mbr->min_y, node->parent->mbr->max_x, node->parent->mbr->max_y);
			else
				rand_mbr = random_mbr(0, 0, MAX_RAND_NUM, MAX_RAND_NUM);

			index_record *new_ir = initialize_ir(rand_mbr);

			// In a real-world scenario these would contain pointers to database tuples, but in our simulation there is no need for that
			new_ir->child = NULL;
			
			add_member(node, new_ir);

		}
		
		return;
	}

	for (i = 0; i < node->max_members * TREE_DENSITY; i++) {

		// If you are at the root level, generate an MBR within the entire specified range (from (0, 0) to (MAX_RAND_NUM, MAX_RAND_NUM)
		// If not at the root level, generate an MBR within the bounds of the parent
		MBR *rand_mbr;

		if (current_level == 0)
			rand_mbr = random_mbr(0, 0, MAX_RAND_NUM, MAX_RAND_NUM);
		else
			rand_mbr = random_mbr(node->parent->mbr->min_x, node->parent->mbr->min_y, node->parent->mbr->max_x, node->parent->mbr->max_y);
		
		index_record *new_ir = initialize_ir(rand_mbr);
		
		r_tree_node *child_node = initialize_rt(node->max_members);
		new_ir->child = child_node;
		child_node->parent = new_ir;
		
		add_member(node, new_ir);

		generate_random_tree(child_node, max_levels, current_level + 1);
	}
}


// Pass the root of the tree that you want to print along with 0
void print_tree(r_tree_node *node, int current_level) {

	int i, j;

	for (i = 0; i < node->num_members; i++) {
		for (j = 0; j < current_level; j++) {
			printf("-----");
		}
		MBR *curr_mbr = node->index_records[i]->mbr;
		printf(">N%dR%d:(min_x: %lf, min_y: %lf, max_x: %lf, max_y: %lf)\n", node->index, i, curr_mbr->min_x, curr_mbr->min_y, curr_mbr->max_x, curr_mbr->max_y);
		
		if (!is_leaf(node))
			print_tree(node->index_records[i]->child, current_level + 1);
	}

}

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



void free_tree(r_tree_node *node) {
	int i;
	for (i = 0; i < node->num_members; i++) {
		if (node->index_records[i]->child != NULL)
			free_tree(node->index_records[i]->child);
		

		free(node->index_records[i]->mbr);
		free(node->index_records[i]);
	}
	
	if (node->parent != NULL)
		node->parent->child = NULL;
	free(node->index_records);
	free(node);
}




void print_tree_specs() {
	if (current_tree_size < pow(2, 10))
		printf("Tree size: %ld bytes\n", current_tree_size);
	else if (current_tree_size < pow(2, 20))
		printf("Tree size: %ld kilobytes\n", (long int) (current_tree_size/pow(2, 10)));
	else if (current_tree_size < pow(2, 30))
		printf("Tree size: %ld megabytes\n", (long int) (current_tree_size/pow(2, 20)));
	printf("Num nodes: %ld\n", num_tree_nodes);
}



/* Does a pre-order traversal of the R-tree, writing each index record in the format:
* node_number,index_record_number,min_x,min_y,max_x,max_y,level
*/
void write_tree_pre_order(FILE *f, r_tree_node *root, int level) {
	int i;
	for (i = 0; i < root->num_members; i++) {
		
		index_record *curr_ir = root->index_records[i];
		MBR *curr_mbr = curr_ir->mbr;
	
		fprintf(f, "%d,%d,%lf,%lf,%lf,%lf,%d\n", root->index, i, curr_mbr->min_x, curr_mbr->min_y, curr_mbr->max_x, curr_mbr->max_y, level);
		if (curr_ir->child != NULL)
			write_tree_pre_order(f, curr_ir->child, level + 1);
	}
}


void write_tree_to_csv(char *filepath, r_tree_node *root) {
	if (strlen(filepath) < 4 || filepath == NULL) {
		fprintf(stderr, "Please provide a .csv file to write the tree data to\n");
		return;
	}
	int last_index = strlen(filepath) - 1;
	if (filepath[last_index] != 'v' || filepath[last_index - 1] != 's' || filepath[last_index - 2] != 'c' || filepath[last_index - 3] != '.') {
		fprintf(stderr, "Please provide a .csv file to write the tree data to\n");
		return;
	}
	FILE *f = fopen(filepath, "w+");
	write_tree_pre_order(f, root, 0);
	fclose(f);
}


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
        			clock_gettime(CLOCK_MONOTONIC, &ts_end);

        			elapsed = ts_end.tv_sec - ts_begin.tv_sec;
        			elapsed += (ts_end.tv_nsec - ts_begin.tv_nsec) / 1000000000.0;

        			printf("Sequential algorithm found node for insertion: N%d\n", insertion_rt_node_1->index);
        			printf("Time taken: %lf seconds\n", elapsed);

			} else {
				
	        		clock_gettime(CLOCK_MONOTONIC, &ts_begin);
       	 			insertion_rt_node_2 = choose_leaf_parallel(root, insertion_ir, num_threads);
       		 		clock_gettime(CLOCK_MONOTONIC, &ts_end);

        			elapsed = ts_end.tv_sec - ts_begin.tv_sec;
        			elapsed += (ts_end.tv_nsec - ts_begin.tv_nsec) / 1000000000.0;
        			printf("Parallel algorithm with %d threads found node for insertion: N%d\n", num_threads, insertion_rt_node_2->index);
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



