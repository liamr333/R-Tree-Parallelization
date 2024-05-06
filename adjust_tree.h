#ifndef _adjust_tree_h
#define _adjust_tree_h

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <unistd.h>

/* Pass this function the r_tree_node in which you are inserting an index_record
* and the index record you are inserting. This will recursively expand the minimum bounding rectangles
* of the index_records above the index_record that you are inserting so that all MBR's above will be
* valid and include all child MBRs
*/
void adjust_tree(r_tree_node *rt, index_record *ir);


#endif
