/* 
 * @author Nathaniel Wendt | Dan Rahm | Chris King
 * @file Final Project : final.c
 * @Program splits up a matrix, a, row by row and sends them to slave processes
 * @to be multiplied by another matrix b that is sent to the slave processes in its entirety.
 * @Each process then creates a given number of threads to calculate its block of memory.
 * @The results are then sent back to the master process to be combined and outputted if necessary.
*/ 
#include <stdio.h> 
#include <mpi.h> 
#include <stdlib.h> 
#include <sys/types.h>
#include <string.h>
#include <pthread.h>

//command line parameters
int nthreads;
int blocksize;
int n;

int* sendrecblock[2];
int* b;
pthread_mutex_t templock;
int* tempblock;

//Handles the master process functionality
//The master process maintains a pool of tasks and sends out each task of size
//blocksize, to each requesting process.  The master process then combines the results
//and prints the results if necessary.  Also, for testing, the master process 
//controls the timing of the computation time necessary to compute the blocks
void MasterProcess(int n, int* b, int blocksize);

//Handles the slave/child process functionality
//Each child process creates nthreads number of threads as dictated from command
//line.  It then combines the results from each thread and sends the values to the master process.
//While this is happening, the process requests a new task from the master process
void ChildProcess(int n, int* b, int blocksize);

//Prints the three matrices, a,b,c
void PrintMatrices(int n, int* a, int* b, int* c);

//Computes a given block based on the threadid value
void *ThreadComputeBlock(int threadid);

int main(int argc, char *argv[]) 
{ 
	int rank, size, i; 

	MPI_Init(&argc, &argv); 
	MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
	MPI_Status status;
	
	n = atoi(argv[1]);
   blocksize = atoi(argv[2]);
   nthreads = atoi(argv[3]);
   pthread_mutex_init(&templock, NULL);
  
   if(blocksize % n != 0) //make sure each process computes at least one row
      blocksize = n;
      
   tempblock = (int *) calloc (blocksize, sizeof(int*));
   sendrecblock[0] = (int *) calloc (blocksize, sizeof(int*));
   sendrecblock[1] = (int *) calloc (blocksize, sizeof(int*));
   b = (int *) calloc (n*n, sizeof(int *));
	
	if(rank == 0){
   	for(i = 0; i <= n * n; i++){
         b[i] = i % 20;
      }	   
   }
   MPI_Bcast(b, n * n, MPI_INT, 0, MPI_COMM_WORLD);
	
	if(rank == 0)
	   MasterProcess(n, b, blocksize);
	else
	   ChildProcess(n, b, blocksize);
	  

	MPI_Finalize(); 
	return 0;
}


//Handles the master process functionality
void MasterProcess(int n, int* b, int blocksize)
{
   int* a;
	int* c;
	double t1,t2;

	int size, process, offset, extrablock, upperLimit;
	int i;
	
	MPI_Status status;
	MPI_Comm_size(MPI_COMM_WORLD, &size); 

	a = (int *) calloc (n*n, sizeof(int *));
	c = (int *) calloc (n*n, sizeof(int *));
	
	for(i = 0; i <= n*n; i++){
      a[i] = i % 7;
	}
   
	t1 = MPI_Wtime(); //start timing
	
	//send initial blocks to each process
	for(process = 1; process < size; process++){
   	offset = (process - 1) * blocksize;
      //memcpy(sendrecblock[0], a + offset, n * sizeof(int) );
      MPI_Send(a + offset, blocksize, MPI_INT, process, process - 1, MPI_COMM_WORLD);
	}
	

	upperLimit = (n * n / blocksize);
	for(i = (size - 1); i < upperLimit + (size - 1); i++){
   	offset = i * blocksize;
      MPI_Recv(sendrecblock[i % 2], blocksize, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);      
      memcpy(c + (status.MPI_TAG * blocksize), sendrecblock[i % 2], blocksize * sizeof(int) );
      if(i < upperLimit){
         //memcpy(sendrecblock[(i + 1) % 2], a + offset, n * sizeof(int) );
         MPI_Send(a + offset, blocksize, MPI_INT, status.MPI_SOURCE, i, MPI_COMM_WORLD);
      }
   }
	
	//tell slaves that they can stop requesting blocks
	for(process = 1; process < size; process++){
      MPI_Send(sendrecblock[0], blocksize, MPI_INT, process, n * n, MPI_COMM_WORLD);
	}
	
   t2 = MPI_Wtime(); //stop timing
   printf("Elapse time is %f seconds\n", t2 - t1);
   
   if(n <= 16)
      PrintMatrices(n,a,b,c);
}



//Handles the slave/child process functionality
void ChildProcess(int n, int* b, int blocksize)
{
   int size, rank, i, j, t, row, rc, col, block, offset, result;
   
   MPI_Request reqs[2]; 
   MPI_Status status;
   MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
   MPI_Comm_size(MPI_COMM_WORLD, &size);
   pthread_t threads[nthreads]; 

   //receive first row
   MPI_Irecv(sendrecblock[0], blocksize, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[0]);
   MPI_Wait(&reqs[0], &status);
   
   i = 0;
   while(status.MPI_TAG != n * n){
      MPI_Irecv(sendrecblock[(i + 1) % 2], blocksize, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[0]);  
      //compute c block
      for(t = 0; t < blocksize; t++){
         tempblock[t] = 0;  
      }
      
      for(t = 0; t < nthreads; t++){      
         rc = pthread_create(&threads[t], NULL, ThreadComputeBlock, (void *)t + ((i+1) * nthreads)); 
         if (rc){ 
            printf("ERROR: return error from pthread_create() is %d\n", rc); 
            exit(-1);
         }
      } 
   
      for(t = 0; t<nthreads; t++){ 
         rc = pthread_join(threads[t], NULL); 
         if (rc){ 
            printf("ERROR: return error from pthread_join() is %d\n", rc); 
            exit(-1); 
         }       
      }
      MPI_Isend(tempblock, blocksize, MPI_INT, 0, status.MPI_TAG, MPI_COMM_WORLD, &reqs[1]);

      i++;
      
      //count down because reqs[0] must be last value checked for the while loop because status
      //is overwritten each time MPI_Wait is called
      for(j = 1; j >= 0; j--){
         MPI_Wait(&reqs[j], &status);
      }
   }
}

void *ThreadComputeBlock(int threadid)
{
   int block, col, row, result, location, i, rowblock, extrablocks, numRows, threadsperline, multiple;
   location = threadid % nthreads;
   i = ((threadid - location) / nthreads) - 1;
   
   rowblock = blocksize / nthreads;
   extrablocks = blocksize - (rowblock * nthreads);
   
   if(blocksize / n > nthreads)
      numRows = n;
   else if(threadid == nthreads - 1) // last thread gets extra
      numRows = rowblock + extrablocks;
   else
      numRows = rowblock;
   
   multiple = 0;
   do{ // continue running while each thread has more than one row to process
      for(col = 0; col < n; col++){
         result = 0;
         for(row = rowblock * location; row < (rowblock * location) + numRows; row++){
            result += sendrecblock[i % 2][row + (multiple * n)] * b[n * (row % n) + col]; //col % n??
         }
         threadsperline = nthreads / (blocksize / n);
         if(threadsperline == 0)
            threadsperline = 1;
   
         tempblock[col               // cannot simplify below, need integer divide
                   +  ((location / threadsperline) * rowblock * threadsperline) //mult. threads per row
                   + (multiple * n)] += result; //multiple rows per thread
      }
     
      multiple++;
   }  while (multiple < (blocksize / n) / nthreads);
   
   pthread_exit(0);
}


//Prints the three matrices, a,b,c
void PrintMatrices(int n, int* a, int* b, int* c)
{
   int i;
   printf("\n\nA: \n");
   for(i = 0; i < n * n; i++){
      if(i % n == 0)
         printf("\n");
      if(a[i] < 10)
         printf("   %d ",a[i]);
      else if(a[i] < 100)
         printf("  %d ",a[i]);
      else
         printf(" %d ",a[i]);
   }
   
   printf("\n\nB: \n");
   for(i = 0; i < n * n; i++){
      if(i % n == 0)
         printf("\n");
      if(b[i] < 10)
         printf("   %d ",b[i]);
      else if(b[i] < 100)
         printf("  %d ",b[i]);
      else
         printf(" %d ",b[i]);
   }
   
   printf("\n\nC: \n");
   for(i = 0; i < n * n; i++){
      if(i % n == 0)
         printf("\n");
      if(c[i] < 10)
         printf("   %d ",c[i]);
      else if(c[i] < 100)
         printf("  %d ",c[i]);
      else if(c[i] < 1000)
         printf(" %d ",c[i]);
      else
         printf("%d ",c[i]);
   }
   printf("\n\n");
  
}
