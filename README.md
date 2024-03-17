# R-Tree-Parallelization
Parallelizing insertion into an R-Tree using POSIX pthreads

Running ./main a b will test and write all times for insertion into the "output_times.csv" file for a randomly generated R-Tree with a index records per node and with a depth of b. Don't worry about generated trees that are too large, as the program will terminate if any tree exceeds a certain size (4GB is the default).
