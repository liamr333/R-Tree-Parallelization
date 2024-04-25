#include "math_utils.h"

// Generate a random double value that is greater than min and less than max
// Used to make randomly generated R-trees valid
double random_within_range(double min, double max) {
    	double range = max - min;
	double rand_val = min + ((double)rand() / RAND_MAX) * range;

	return rand_val;
}

