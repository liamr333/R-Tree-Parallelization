#include "r_tree.h"
#include "math_utils.h"

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


// Given two MBRs, see what the area of their merged MBR is
double get_merged_area(MBR *mbr1, MBR *mbr2) {

	double new_width = fmax(mbr1->max_x, mbr2->max_x) - fmin(mbr1->min_x, mbr2->min_x);
        double new_height = fmax(mbr1->max_y, mbr2->max_y) - fmin(mbr1->min_y, mbr2->min_y);

	return new_width * new_height;
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

// Removes the index record in r at index index. Returns NULL if index invalid or r empty
index_record *remove_index_record(r_tree_node *r, int index) {
	if (r->num_members == 0 || index >= r->num_members)
		return NULL;

	int i;

	index_record *ir = r->index_records[index];

	// Move all index records after ir back by one to fill in the gap

	for (i = index + 1; i < r->num_members; i++)
		r->index_records[i- 1] = r->index_records[i];


	r->num_members--;

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
