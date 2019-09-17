#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
        int seed, num_rows, num_columns;
        int i,j;
        if(argc != 4)
        {
          fprintf(stderr,"USAGE: ./a.out <seed> <num_rows> <num_columns>\n");
          exit(0);
        }
        seed = atoi(argv[1]);
        num_rows = atoi(argv[2]);
        num_columns = atoi(argv[3]);
        srand (seed);
        for(i=0; i<num_rows; i++)
        {
          for(j=0; j<num_columns-1; j++)
          {
            fprintf(stdout,"%d ", (int)(random()%2));
          }
          fprintf(stdout,"%d\n", (int)(random()%2));
        }
				return 0;
}

