#ifndef _r_tree_h
#define _r_tree_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

// Trees cannot exceed 4GB in total size
#define MAX_TREE_SIZE 4294967296


// Percentage of index_records in r_tree_node's as opposed to empty slots when randomly generating r-trees
#define TREE_DENSITY 1
#define MAX_RAND_NUM 100

// Slow and not recommended for large trees
#define SAVE_TO_CSV false

#define PRINT_TREE false
#define PRINT_TREE_SPECS true
#define NUM_CORES 12


//int next_node_index = 0;
//long current_tree_size = 0;
//long num_tree_nodes = 0;


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


double get_area(MBR *mbr);


// Create a minimum bounding rectangle by providing a lower left coordinate (min_x, min_y)
// and an upper-right hand coordinate (max_x, max_y)
MBR *create_mbr(int min_x, int min_y, int max_x, int max_y);


// Generate a minimum bounding rectangle with each coordinate
// min_min_x, min_min_y, max_max_x, and max_max_y provide the bounds within which the randomly generated MBR
// can be generated. Otherwise, it may be too big and not fit within its parent
MBR *random_mbr(double min_min_x, double min_min_y, double max_max_x, double max_max_y);


double get_merged_area(MBR *mbr1, MBR *mbr2);

// Test and see what the area increase to an MBR would be if you add another child to it
double get_area_increase(MBR *original, MBR *new_child);


index_record *initialize_ir(MBR *mbr);


r_tree_node *initialize_rt(int max_members);


// Returns true if successfully added a new index record to the r_tree_node, false if you have reached max capacity and need to split
bool add_member(r_tree_node *host_node, index_record *new_member);

// Returns true if the r_tree_node is a bottom-level node
// You can see that this is true when the node's index_record's point to NULL
bool is_leaf(r_tree_node *node);


// Generates a random r-tree where each r_tree_node has r_tree_node->max_members / 2 entries
// Pass current_level as 0
void generate_random_tree(r_tree_node *node, int max_levels, int current_level);


// Pass the root of the tree that you want to print along with 0
void print_tree(r_tree_node *node, int current_level);


void print_tree_specs();


void free_tree(r_tree_node *node);


/* Does a pre-order traversal of the R-tree, writing each index record in the format:
* node_number,index_record_number,min_x,min_y,max_x,max_y,level
*/
void write_tree_pre_order(FILE *f, r_tree_node *root, int level);


void write_tree_to_csv(char *filepath, r_tree_node *root);


#endif
