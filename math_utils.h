#ifndef _math_utils_h
#define _math_utils_h

#include <stdlib.h>
#include <math.h>
#include <float.h>

#define MAX_RAND_NUM 100



// Generate a random double value that is less than min and greater than max
// Used to make randomly generated R-trees valid
double random_within_range(double min, double max);


#endif
