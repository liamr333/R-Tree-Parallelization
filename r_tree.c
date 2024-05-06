/*
Note to readers:


Please read Antonin Guttman's original paper on R-Trees before approaching this code.
This is a partial implementation of the data structures and algorithms that he desribed in his paper.
It also includes some custom algorithms by the writer of the code (Liam Ralph)
*/

#include "r_tree.h"
#include "math_utils.h"
#include "adjust_tree.h"
#include "pick_seeds.h"
#include "linear_split.h"
#include "choose_leaf.h"

int next_node_index = 0;
long current_tree_size = 0;
long num_tree_nodes = 0;



double get_area(MBR *mbr) {
        return (mbr->max_x - mbr->min_x) * (mbr->max_y - mbr->min_y);
}


// Create a minimum bounding rectangle by providing a lower left coordinate (min_x, min_y)
// and an upper-right hand coordinate (max_x, max_y)

MBR *create_mbr(int min_x, int min_y, int max_x, int max_y) {

        MBR *new_mbr = (MBR *)malloc(sizeof(MBR));

	current_tree_size += sizeof(MBR);

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

// Returns a randomly generated MBR of at most width 1 and at most height 1 within the given bounds
// both the position, height, and width of the MBR are random, but it is guaranteed to be fully confined
// within min_min_x, min_min_y, max_max_x, and max_max_y
MBR *random_small_mbr(double min_min_x, double min_min_y, double max_max_x, double max_max_y) {
	MBR *new_mbr = (MBR*)malloc(sizeof(MBR));

	current_tree_size = current_tree_size + sizeof(MBR);

	if (current_tree_size > MAX_TREE_SIZE) {
		fprintf(stderr, "Max tree size exceeded. Exiting program\n");
		exit(1);
	}

	if (new_mbr == NULL)
		exit(1);

	double x1 = random_within_range(min_min_x, max_max_x);
	double y1 = random_within_range(min_min_y, max_max_y);
	double x2 = fmin(random_within_range(x1, x1 + 1), max_max_x);
	double y2 = fmin(random_within_range(y1, y1 + 1), max_max_y);

	new_mbr->min_x = x1;
	new_mbr->min_y = y1;
	new_mbr->max_x = x2;
	new_mbr->max_y = y2;

	return new_mbr;
}

// Given two MBRs, see what the area of their merged MBR is
double get_merged_area(MBR *mbr1, MBR *mbr2) {

	double new_width = fmax(mbr1->max_x, mbr2->max_x) - fmin(mbr1->min_x, mbr2->min_x);
        double new_height = fmax(mbr1->max_y, mbr2->max_y) - fmin(mbr1->min_y, mbr2->min_y);

	return new_width * new_height;
}


// Given an MBR parent and an MBR child where child is being added as a child to parent,
// check if parent fully contains child
bool fully_contains(MBR *parent_mbr, MBR *child_mbr) {
	if (child_mbr->min_x < parent_mbr->min_x)
		return false;
	if (child_mbr->min_y < parent_mbr->min_y)
		return false;
	if (child_mbr->max_x > parent_mbr->max_x)
		return false;
	if (child_mbr->max_y > parent_mbr->max_y)
		return false;
	return true;
}


// Given an MBR parent and an MBR child whre child is being added as a child to parent,
// expand parent to fully encompass child
void expand_parent(MBR *parent_mbr, MBR *child_mbr) {
	if (child_mbr->min_x < parent_mbr->min_x)
		parent_mbr->min_x = child_mbr->min_x;
	if (child_mbr->min_y < parent_mbr->min_y)
		parent_mbr->min_y = child_mbr->min_y;
	if (child_mbr->max_x > parent_mbr->max_x)
		parent_mbr->max_x = child_mbr->max_x;
	if (child_mbr->max_y > parent_mbr->max_y)
		parent_mbr->max_y = child_mbr->max_y;
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

        return new_area - original_area;
}

// Expands the area of mbr to include child_mbr
// Does nothing if child_mbr is fully contained withing mbr
void expand_mbr(MBR *mbr, MBR *child_mbr) {
	if (child_mbr->min_x < mbr->min_x)
		mbr->min_x = child_mbr->min_x;
	if (child_mbr->min_y < mbr->min_y)
		mbr->min_y = child_mbr->min_y;
	if (child_mbr->max_x > mbr->max_x)
		mbr->max_x = child_mbr->max_x;
	if (child_mbr->max_y > mbr->max_y)
		mbr->max_y = child_mbr->max_y;
}

// Returns the overlapping area of two minimum bounding rectangles
double get_overlapping_area(MBR *mbr1, MBR *mbr2) {
        double x_distance = fmin(mbr1->max_x, mbr2->max_x) - fmax(mbr1->min_x, mbr2->min_x);
        double y_distance = fmin(mbr1->max_y, mbr2->max_y) - fmax(mbr1->min_y, mbr2->min_y);

        if (x_distance <= 0 || y_distance <= 0)
            return 0.0;

	double overlapping_area = x_distance * y_distance;

	return overlapping_area;
}


index_record *initialize_ir(MBR *mbr) {
	index_record *ir = (index_record *)malloc(sizeof(index_record));

	current_tree_size += sizeof(index_record);

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

// Given an MBR as a parameter, create an identical copy of that MBR at a different location in memory
MBR *copy_mbr(MBR *original_mbr) {
	MBR *copy = (MBR*)malloc(sizeof(MBR));

	current_tree_size += sizeof(MBR);

	if (copy == NULL) {
		fprintf(stderr, "Malloc failed in original_mbr(). Exiting program\n");
		exit(1);
	}

	copy->min_x = original_mbr->min_x;
	copy->min_y = original_mbr->min_y;
	copy->max_x = original_mbr->max_x;
	copy->max_y = original_mbr->max_y;

	return copy;
}


r_tree_node *initialize_rt(int max_members) {
	r_tree_node *rt = (r_tree_node *)malloc(sizeof(r_tree_node));

	current_tree_size += sizeof(r_tree_node);
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
	if (is_full(host_node))
		return false;

	new_member->index = host_node->num_members;
	host_node->index_records[host_node->num_members] = new_member;
	host_node->num_members++;
	new_member->host = host_node;

	// expand parent index record's MBR if necessary
	if (!is_parent(host_node)) {
		MBR *parent_mbr = host_node->parent->mbr;
		MBR *child_mbr = new_member->mbr;
		expand_mbr(parent_mbr, child_mbr);
	}

	return true;
}



// Removes the index record in r at index index. Returns NULL if index invalid or r empty
index_record *remove_index_record(r_tree_node *rt, int index) {
	if (rt->num_members == 0 || index >= rt->num_members)
		return NULL;

	int i;

	index_record *ir = rt->index_records[index];
	rt->index_records[index] = NULL;

	// Move all index records after ir back by one to fill in the gap

	for (i = index + 1; i < rt->num_members; i++) {
		// Change index of index_record to reflect new position
		rt->index_records[i]->index--;
		rt->index_records[i- 1] = rt->index_records[i];
	}

	rt->index_records[rt->num_members - 1] = NULL;
	rt->num_members--;

	return ir;
}

// Moves the index record in r1 at index index to r2. Returns false if index invalid,
// r1 empty, or r2 full
bool move_index_record(r_tree_node *r1, r_tree_node *r2, int index) {
	index_record *ir = remove_index_record(r1, index);

	if (ir == NULL)
		return false;

	return add_member(r2, ir);
}


// Returns true if the r_tree_node is a bottom-level node
bool is_leaf(r_tree_node *node) {
	return node->index_records[0]->child == NULL;
}


// Returns true if the r_tree_node is a top-level node
bool is_parent(r_tree_node *node) {
	return node->parent == NULL;
}

// Returns true if there is the max number of index_record's in your r_tree_node
bool is_full(r_tree_node *node) {
	return node->num_members == node->max_members;
}


// (Used for when you have already found the leaf-level r_tree_node rt to insert your index_record ir into
// Insertion function that can be parallized or not based on arguments
// Finds the optimal insertion to minimize MBR overlap
// The node splitting algorithm was made up by me
// You need to pass root because it might change if a new root is created
void insert_at_node(r_tree_node *rt, index_record *ir, r_tree_node **root, int num_threads) {

	// If the rt node has room, just add the index record like normal
	if (!is_full(rt)) {
		add_member(rt, ir);
		adjust_tree(rt, ir);
	} else {

		double biggest_waste;
		int *seed_indices;


		if (num_threads > 1)
			seed_indices = pick_seeds_parallel(rt, ir, num_threads);
		else
			seed_indices = pick_seeds_sequential(rt->index_records, rt->num_members, &biggest_waste);


		index_record *seed_1 = rt->index_records[seed_indices[0]];
		index_record *seed_2 = rt->index_records[seed_indices[1]];

		MBR *ir_1_mbr = copy_mbr(seed_1->mbr);
		MBR *ir_2_mbr = copy_mbr(seed_2->mbr);

		index_record *ir_1 = initialize_ir(ir_1_mbr);
		index_record *ir_2 = initialize_ir(ir_2_mbr);

		ir_1->child = initialize_rt(rt->max_members);
		ir_2->child = initialize_rt(rt->max_members);

		ir_1->child->parent = ir_1;
		ir_2->child->parent = ir_2;

		// Reminder: We don't have to manually put the seeds in ir_1->child and ir_2->child because
		// linear_split will do that for us

		// Split the rt node. Now every index_records previously in rt will now be in either
		// ir_1->child->index_records or ir_2->child->index_records

		if (num_threads > 1)
			linear_split_parallel(rt, ir_1, ir_2, num_threads);
		else
			linear_split_sequential(rt, ir_1, ir_2);

		// Now add the index record ir that is being added to either ir_1->child or ir_2->child,
		// whichever is optimal
		double expansion_1 = get_area_increase(ir->mbr, ir_1->mbr);
		double expansion_2 = get_area_increase(ir->mbr, ir_2->mbr);

		// Since one of the new index_records points to an r_tree_node with the new inserted index_record ir,
		// we have to keep track of it in order to perform adjust tree on its host r_tree_node
		index_record *ir_expand = ir_1;
		index_record *ir_same = ir_2;

		if (expansion_1 < expansion_2) {
			add_member(ir_1->child, ir);
		} else {
			add_member(ir_2->child, ir);

			// By default, we make ir_expand = ir_1. Since we are actually adding the new index_record
			// to ir_2, we switch ir_expand and ir_same
			index_record *temp = ir_expand;
			ir_expand = ir_same;
			ir_same = temp;
		}

		if (rt->parent != NULL) {

			r_tree_node *parent_r_tree_node = rt->parent->host;
			remove_index_record(parent_r_tree_node, rt->parent->index);
			add_member(parent_r_tree_node, ir_same);

			// free this r_tree_node since it will not be in the tree anymore
			rt->parent = NULL;

			// TO-DO: Free stuff that isn't being used anymore
			int i;

			// make sure that this old r_tree_node that we won't use anymore isn't pointing to any
			// index records that are still going to be in the tree
			for (i = 0; i < rt->num_members; i++) {
				rt->index_records[i] = NULL;
			}

			rt->num_members = 0;
			free_tree(rt);

			insert_at_node(parent_r_tree_node, ir_expand, root, num_threads);
		} else {
			r_tree_node *next_parent = initialize_rt(rt->max_members);

			add_member(next_parent, ir_1);
			add_member(next_parent, ir_2);

			*root = next_parent;
		}
	}
}



// General insertion function
// You need to pass a pointer to a pointer of the root in case the root changes to a new root during the insertion process
void insert(r_tree_node **root, index_record *ir, int num_threads) {

	if (num_threads > (*root)->max_members) {
		fprintf(stderr, "Cannot use more threads than number of max_members in your r_tree_node\n");
		fprintf(stderr, "Aborting insertion\n");
		exit(1);
	}

        r_tree_node *insertion_leaf;

        if (num_threads > 1)
                insertion_leaf = choose_leaf_parallel(*root, ir, num_threads);
        else
                insertion_leaf = choose_leaf_sequential(*root, ir);

        insert_at_node(insertion_leaf, ir, root, num_threads);
}

// Given an r_tree_node, ensures that its parent index_record has an MBR that is the minimum bounding rectangle of all children MBR's
// Used so that generate_randoom_tree does not create MBRs that are not actually MBRs
void validate_node(r_tree_node *rt) {
	if (rt->parent == NULL || rt->num_members == 0) {
		return;
	}

	// For finding the four extrema of all index_records in rt
	double min_min_x, min_min_y, max_max_x, max_max_y;

	min_min_x = rt->index_records[0]->mbr->min_x;
	min_min_y = rt->index_records[0]->mbr->min_y;
	max_max_x = rt->index_records[0]->mbr->max_x;
	max_max_y = rt->index_records[0]->mbr->max_y;

	int i;

	for (i = 0; i < rt->num_members; i++) {
		MBR *curr_mbr = rt->index_records[i]->mbr;

		if (curr_mbr->min_x < min_min_x)
			min_min_x = curr_mbr->min_x;
		if (curr_mbr->min_y < min_min_y)
			min_min_y = curr_mbr->min_y;
		if (curr_mbr->max_x > max_max_x)
			max_max_x = curr_mbr->max_x;
		if (curr_mbr->max_y > max_max_y)
			max_max_y = curr_mbr->max_y;

	}

	rt->parent->mbr->min_x = min_min_x;
	rt->parent->mbr->min_y = min_min_y;
	rt->parent->mbr->max_x = max_max_x;
	rt->parent->mbr->max_y = max_max_y;
}


// Generates a random r-tree where each r_tree_node has r_tree_node->max_members * TREE_DENSITY entries
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

		validate_node(node);

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

	validate_node(node);

}


// Pass the root of the tree that you want to print along with 0
void print_tree(r_tree_node *node, int current_level) {

	if (node == NULL)
		fprintf(stderr, "Erroneously recursed to null node\n");

	int i, j;

	for (i = 0; i < node->num_members; i++) {

		for (j = 0; j < current_level; j++) {
			printf("-----");
		}

		index_record *curr_ir = node->index_records[i];
		MBR *curr_mbr = curr_ir->mbr;

		printf(">(at %p): N%dR%d:(min_x: %lf, min_y: %lf, max_x: %lf, max_y: %lf)\n", curr_ir, node->index, curr_ir->index, curr_mbr->min_x, curr_mbr->min_y, curr_mbr->max_x, curr_mbr->max_y);

		if (!is_leaf(node))
			print_tree(node->index_records[i]->child, current_level + 1);

	}

}

// Pass the root node of your tree to this function and it will free the entire tree
void free_tree(r_tree_node *node) {
	int i;

	for (i = 0; i < node->num_members; i++) {

		if (node->index_records[i]->child != NULL) {
			current_tree_size -= sizeof(r_tree_node);
			free_tree(node->index_records[i]->child);
		}

		current_tree_size -= sizeof(MBR);
		current_tree_size -= sizeof(index_record);
		free(node->index_records[i]->mbr);
		free(node->index_records[i]);
	}

	if (node->parent != NULL)
		node->parent->child = NULL;

	current_tree_size -= sizeof(node->index_records);
	current_tree_size -= sizeof(node);
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

	fprintf(f, "Root Index,Index Record Index, Min X, Min Y, Max X, Max Y, Level\n");

	write_tree_pre_order(f, root, 0);

	fclose(f);
}
