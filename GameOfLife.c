#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <mpi.h>

int rank;
int size;
int my_ChunkSize;
int my_Rows;
long randNosCounter=0;
int turn = 0;//to alter between old_partial_Universe and new_partial_Universe

//double t1=0,t2=0;//to capture time

int * full_Universe = NULL;//rank 0 array to keep track of all elements
char * number;//to read the numbers from input file
int * rbuf_Count = NULL;//rank 0 array to keep the chunksize(total number of elements) in each process including column 0s on both side
int * displs = NULL;//rank 0 array to guide from where to start receiving elements from other processes

void seed_By_Input(int seed);//to seed srand
void distributed_Storage_Allocate(int ** array, int rows, int columns);//to allocate space for new_partial_Universe and old_partial_Universe for each process
void initialize_Full(int * array, int rows, int columns);//to initialize the full_Universe with 0s
void fill_Initial_Full(int * array, int rows, int columns);//if input file is not given fill the full_Universe with random no. generator
int countNeighbours(int * array, int m, int n, int columns);//count the neighbours of cell[m][n]
void play_game_of_life(int * old_partial_Universe, int * new_partial_Universe, int rows, int columns, int generations);//start game of life and produce next generations
void print_full_Universe(int * array, int rows,  int columns);//to print the full_Universe
//void print_time(double tm);

int main(int argc, char **argv)
{
	int threads_per_proc;
	int seed;
	int rows;
	int columns;
	int generations;
	char * filename;

	int * old_partial_Universe;
	int * new_partial_Universe;

	int i,j,k;

	MPI_Init(&argc, &argv);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);


	if(argc != 6 && argc != 7)
	{
		if(rank == 0)
		{
			printf("Usage: %s [Threads_Per_Proc] [Seed] [Rows] [Columns] [Generations] [opt-filename]\n", argv[0]);
			exit(0);
		}

	}

	sscanf(argv[1], "%d", &threads_per_proc);
	sscanf(argv[2], "%d", &seed);
	sscanf(argv[3], "%d", &rows );
	sscanf(argv[4], "%d", &columns );
	sscanf(argv[5], "%d", &generations );
	if(argc==7)
	sscanf(argv[6], "%s", filename);

	omp_set_num_threads(threads_per_proc);

	//------------------------------------------------------------------------------------------

	if(rank == 0)
	{
		full_Universe = (int *) malloc (sizeof(int)*(rows*(columns+2)));
		rbuf_Count = (int *) malloc (sizeof(int)*size);
		displs = (int *) malloc (sizeof(int)*size);
		displs[0] = 0;
	}

	//-------------------------------------------------------------------------------------------

	seed_By_Input(seed);

	distributed_Storage_Allocate(&old_partial_Universe, rows, columns);
	distributed_Storage_Allocate(&new_partial_Universe, rows, columns);
  if(rank==0)
  {
    initialize_Full(full_Universe,rows,columns);
  }

  //t1 = MPI_Wtime();

  if(argc==6)
  {
    if(rank==0)
    fill_Initial_Full(full_Universe,rows,columns);
  }

  else if(argc==7)
  {
    MPI_File fp;
    MPI_Offset filesize;
    MPI_File_open(MPI_COMM_WORLD,filename,MPI_MODE_RDONLY,MPI_INFO_NULL,&fp);
    MPI_File_get_size(fp,&filesize);

    if(rank==0)
    {
      j=1;
      k=0;
      number = (char *) malloc (sizeof(char)*filesize);
      MPI_File_read_at(fp,0, number,filesize, MPI_CHAR, MPI_STATUS_IGNORE);//reading the digits as characters into the number array
      for(i=0; i<filesize-1; i++)//iterate through each character and store 1 and 0 into full_Universe
      {
        if(number[i] == '1' || number[i] == '0' )
        {
          full_Universe[k*(columns+2)+(j++)] = number[i] - '0';
          if(j%(columns+1)==0)
          {
            j=1;k++;
          }
        }
      }
    }

    MPI_File_close(&fp);
  }

  int start = (rank==0)?2:1;
  int end = start + my_Rows ;
  int num_of_elements = (end-start)*(columns+2);

  MPI_Gather(&num_of_elements,1,MPI_INT,rbuf_Count,1,MPI_INT,0,MPI_COMM_WORLD);//rank 0 process gets the chunksize(# of elem) of each process in rbuf_Count

  if(rank==0)
  {
    for(i = 1; i< size; i++)
    {
      displs[i] = displs[i-1] + rbuf_Count[i-1];//calculating displacement for each process
    }
  }

  MPI_Scatterv(full_Universe,rbuf_Count,displs,MPI_INT,&(old_partial_Universe[start*(columns+2)+0]),num_of_elements,MPI_INT,0,MPI_COMM_WORLD);


	play_game_of_life(old_partial_Universe, new_partial_Universe, rows, columns, generations);

	if(turn==0)
	{
		MPI_Gatherv(&(old_partial_Universe[start*(columns+2)+0]),num_of_elements,MPI_INT,full_Universe,rbuf_Count,displs,MPI_INT,0,MPI_COMM_WORLD);
	}
	else
	{
		MPI_Gatherv(&(new_partial_Universe[start*(columns+2)+0]),num_of_elements,MPI_INT,full_Universe,rbuf_Count,displs,MPI_INT,0,MPI_COMM_WORLD);
	}

	if(rank==0)
	{
		print_full_Universe(full_Universe,rows,columns+2);
	}
  MPI_Barrier(MPI_COMM_WORLD);
  //t2 = MPI_Wtime();

  if(rank == 0)
  {
    //print_time(t2-t1);
    free(full_Universe);
    free(number);
    free(rbuf_Count);
    free(displs);
  }

  free(old_partial_Universe);
  free(new_partial_Universe);


	MPI_Finalize();

	return 0;
}

void seed_By_Input(int seed)
{
	srand(seed);
}

void distributed_Storage_Allocate (int **array, int rows, int columns)
{
	int i;
	my_ChunkSize = rows/size; //evenly divisible chunk
	if(rows%size!=0)//assigning the remaining rows to intial processes
	{
		int rem = rows%size;
		if(rank < rem)
		{
			my_ChunkSize = my_ChunkSize + 1;
		}
	}

	my_Rows = my_ChunkSize;

	if(size==1)//single process(root) 2 extra rows above and 2 extra rows below
	{
		my_ChunkSize+= 4;
	}

	else if(rank==0 || rank==size-1)//2 extra rows above and 1 extra row below
	{
		my_ChunkSize+= 3;
	}

	else//1 extra row above and 1 extra row below
	{
		my_ChunkSize+= 2;
	}

	*array = (int *) malloc (sizeof(int)*(my_ChunkSize * (columns+2)));//allocating extra 2 columns for side 0s

}

void initialize_Full(int * array, int rows, int columns)
{
	int i,j;

	#pragma omp parallel for
	for(i=0; i<rows; i++)
	{
		for(j=0; j<(columns+2); j++)
		{
			array[i*(columns+2)+j] = 0;
		}
	}
}

void fill_Initial_Full(int * array, int rows, int columns)
{
	int i,j;
	int start = 0;
	int end = rows;

	for( i=start; i< end; i++)
	{
		for(j=1; j<(columns+1); j++)
		{
			//if(rand()%2)
			++randNosCounter;
			if(rand()%2)
			{
				array[i*(columns+2)+j] = 1;
			}
			else
			array[i*(columns+2)+j] = 0;
		}
	}
}

void print_full_Universe(int * array, int rows,int columns)
{
	int i,j;
	for(i=0;i<rows;i++)
	{
		for(j=1;j<(columns-1);j++)
		{
			printf("%d\n", array[i*(columns)+j]);
		}

	}
}

int countNeighbours(int * array, int m, int n, int columns)
{
	int i,j,sum=0;

	for(i=m-1; i<=m+1; i++)
	{
		for(j=n-1; j<=n+1; j++)
		{
			if(array[i*(columns+2)+j]==1)
			{
				sum+=1;
			}
		}
	}
	sum-= array[m*(columns+2)+n];
	return sum;
}

void play_game_of_life(int * old_partial_Universe, int * new_partial_Universe, int rows, int columns, int generations)
{
	int i, j;
	int start = (rank==0)?2:1;
	int end = start + my_Rows ;
	int num_of_elements = (end-start)*(columns+2);

	while(generations != 0)
	{
		--generations;

		if(turn==0)
		{
			if(rank!=size-1)
			{
				MPI_Send(&old_partial_Universe[(my_ChunkSize-2)*(columns+2)+1],columns,MPI_INT,rank+1,4,MPI_COMM_WORLD);
			}

			if(rank!=0)
			{
				MPI_Recv(&old_partial_Universe[0*(columns+2)+1],columns,MPI_INT,rank-1,4,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
			}

			if(rank!=0)
			{
				MPI_Send(&old_partial_Universe[1*(columns+2)+1],columns,MPI_INT,rank-1,4,MPI_COMM_WORLD);
			}

			if(rank!=size-1)
			{
				MPI_Recv(&old_partial_Universe[(my_ChunkSize-1)*(columns+2)+1],columns,MPI_INT,rank+1,4,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
			}

      #pragma omp parallel for collapse(2)
			for( i=start; i< end; i++)
			{
				for(j=1; j<(columns+1); j++)
				{
          int k = i*(columns+2)+j;
					int count = countNeighbours(old_partial_Universe,i,j,columns);
					if(old_partial_Universe[k]==0 && count==3)
						new_partial_Universe[k]=1;
					else if(old_partial_Universe[k]==1 && (count<2 || count>3 ))
						new_partial_Universe[k]=0;
					else
						new_partial_Universe[k] = old_partial_Universe[k];
				}
			}
		}

		else
		{
				if(rank!=size-1)
				{
					MPI_Send(&new_partial_Universe[(my_ChunkSize-2)*(columns+2)+1],columns,MPI_INT,rank+1,3,MPI_COMM_WORLD);
				}

				if(rank!=0)
				{
					MPI_Recv(&new_partial_Universe[0*(columns+2)+1],columns,MPI_INT,rank-1,3,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
				}

				if(rank!=0)
				{
					MPI_Send(&new_partial_Universe[1*(columns+2)+1],columns,MPI_INT,rank-1,3,MPI_COMM_WORLD);
				}

				if(rank!=size-1)
				{
					MPI_Recv(&new_partial_Universe[(my_ChunkSize-1)*(columns+2)+1],columns,MPI_INT,rank+1,3,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
				}

        #pragma omp parallel for collapse(2)
				for( i=start; i< end; i++)
				{
					for(j=1; j<(columns+1); j++)
					{
            int k = i*(columns+2)+j;
						int count = countNeighbours(new_partial_Universe,i,j,columns);
						if(new_partial_Universe[k]==0 && count==3)
							old_partial_Universe[k]=1;
						else if(new_partial_Universe[k]==1 && (count<2 || count>3 ))
							old_partial_Universe[k]=0;
						else
							old_partial_Universe[k] = new_partial_Universe[k];
					}
				}

		}

		turn = (turn+1)%2;
	}
}

// void print_time(double tm)
// {
//     printf("elapsed time = %f seconds\n", tm);
// }
