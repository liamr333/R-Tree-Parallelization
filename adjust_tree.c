#include "r_tree.h"
#include "adjust_tree.h"

/* Pass this function the r_tree_node in which you are inserting an index_record
* and the index record you are inserting. This will recursively expand the minimum bounding rectangles
* of the index_records above the index_record that you are inserting so that all MBR's above will be
* valid and include all child MBRs
*/
void adjust_tree(r_tree_node *rt, index_record *ir) {
	// Stop at the parent
	if (rt->parent == NULL)
		return;

	expand_mbr(rt->parent->mbr, ir->mbr);

	adjust_tree(rt->parent->host, rt->parent);
}
