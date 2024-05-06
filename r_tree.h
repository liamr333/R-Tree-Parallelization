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
	struct r_tree_node *host;
	int index;
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


// Returns a randomly generated MBR of at most width 1 and at most height 1 within the given bounds
// both the position, height, and width of the MBR are random, but it is guaranteed to be fully confined
// within min_min_x, min_min_y, max_max_x, and max_max_y
MBR *random_small_mbr(double min_min_x, double min_min_y, double max_max_x, double max_max_y);


double get_merged_area(MBR *mbr1, MBR *mbr2);

// Test and see what the area increase to an MBR would be if you add another child to it
double get_area_increase(MBR *original, MBR *new_child);

// Expands the area of mbr to include child_mbr
// Does nothing if child_mbr is fully contained withing mbr
void expand_mbr(MBR *mbr, MBR *child_mbr);

// Returns the overlapping area of two minimum bounding rectangles
double get_overlapping_area(MBR *mbr1, MBR *mbr2);


index_record *initialize_ir(MBR *mbr);

// Given an MBR as a parameter, create an identical copy of that MBR at a different location in memory
MBR *copy_mbr(MBR *original_mbr);


r_tree_node *initialize_rt(int max_members);


// Returns true if successfully added a new index record to the r_tree_node, false if you have reached max capacity and need to split
bool add_member(r_tree_node *host_node, index_record *new_member);


// Removes the index record in r at index index. Returns NULL if index invalid or r empty
index_record *remove_index_record(r_tree_node *r, int index);

// Moves the index record in r1 at index index to r2. Returns false if index invalid,
// r1 empty, or r2 full
bool move_index_record(r_tree_node *r1, r_tree_node *r2, int index);


// Returns true if the r_tree_node is a bottom-level node
bool is_leaf(r_tree_node *node);


// Returns true if the r_tree_node is a top-level node
bool is_parent(r_tree_node *node);


// Returns true if there is the max number of index_record's in your r_tree_node
bool is_full(r_tree_node *node);

// (Used for when you have already found the leaf-level r_tree_node rt to insert your index_record ir into
// Insertion function that can be parallized or not based on arguments
// Finds the optimal insertion to minimize MBR overlap
// The node splitting algorithm was made up by me
// You need to pass root because it might change if a new root is created
void insert_at_node(r_tree_node *rt, index_record *ir, r_tree_node **root, int num_threads);

// General insertion function
// You need to pass a pointer to a pointer of the root in case the root changes to a new root during the insertion process
void insert(r_tree_node **root, index_record *ir, int num_threads);

// Given an r_tree_node, ensures that its parent index_record has an MBR that is the minimum bounding rectangle of all children MBR's
// Used so that generate_randoom_tree does not create MBRs that are not actually MBRs
void validate_node(r_tree_node *rt);


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
