README FILE

CPSC/CPEN 435 Parallel Computing Final Project
Nathaniel Wendt
Dan Rahm
Chris King

CONTENTS
I.   How to run the program
II.  Program runtime stipulations

***************************************************

I.   How to run the program
1. Open the directory in ada
2. Type: mpicc final.c -o final -lpthread
	This will compile the file
3. To run the program you need to adjust the parallel.txt file
4. Change the line that reads #PBS -o to the directory of your desired output file
5. Change the line that reads #PBS -l walltime to limit the maximum time the program will run
6. Change the line that reads export PROGRAM="..." to the directory where the program is located
7. Change the line that reads mpirun -p to end with the following format
	Size of matrix (N)  |   Blocksize  | Number of Threads

An example run might be:  512 512 4


***************************************************

II.  Program runtime stipulations

When changing the last item in step 7 above, make sure that blocksize
is a power of two multiple of N.  Also, make sure the number of threads
is a power of 2.  Finally, this program will never finish if only one process
is assigned to be run since once process is assigned to send parts
of the matrix to other processes.  Make more than one process is assigned
in this program.

