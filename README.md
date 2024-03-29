# R-Tree-Parallelization
Parallelizing insertion into an R-Tree using POSIX pthreads

Running ./main m n will test and write all times for insertion into the "output_times.csv" file for a randomly generated R-Tree with m index records per node and with a depth of n. Don't worry about generated trees that are too large, as the program will terminate if any tree exceeds a certain size (4GB is the default).

The algorithm presented (choose_leaf_parallel) is a parallelized verion of Antonin Guttman's ChooseLeaf algorithm that he published in his 1984 paper "R-trees: A dynamic index structure for spatial searching".

The provided output_times.csv and analysis Jupyter Workbook are based on data I derived by running main 4096 2; main 256 3; main 8192 2;. If you want to generate your own data, delete the output_times.csv file before running main so that it will write to a new output_times.csv file.
