#ifndef __USE_FILE_OFFSET64
#define __USE_FILE_OFFSET64
#endif
#define TEST_NRFS_IO
//#define TEST_RAW_IO
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define BUFFER_SIZE 0x1000000
int myid, file_seq;
int numprocs;
char buf[BUFFER_SIZE];
int mask = 0;
int collect_time(int cost)
{
	int i;
	char message[8];
	MPI_Status status;
	int *p = (int*)message;
	int max = cost;
	for(i = 1; i < numprocs; i++)
	{
		MPI_Recv(message, 8, MPI_CHAR, i, 99, MPI_COMM_WORLD, &status);
		if(*p > max)
			max = *p;
	}
	return max;
}

void write_test(int size, int op_time)
{
	char path[255];
	int i;
	double start, end, rate, num;
	int time_cost;
	char message[8];
	int *p = (int*)message;

	/* file open */
	sprintf(path, "/ssd/file_%d", file_seq);
        FILE *fp = fopen(path, "wb");
        //int fd = open(path, O_RDWR | O_CREAT, 0666);
	printf("create file: %s\n", path);
	memset(buf, 'a', BUFFER_SIZE);

	MPI_Barrier ( MPI_COMM_WORLD );
	long offset = 0;
        int n;
	start = MPI_Wtime();
	for(i = 0; i < op_time; i++)
	{
               //write(fd, buf, size);
               fwrite(buf, sizeof(char), size, fp);
	}
        fflush(fp);
	end = MPI_Wtime();

	MPI_Barrier ( MPI_COMM_WORLD );

	*p = (int)((end - start) * 1000000);

	if(myid != 0)
	{
		MPI_Send(message, 8, MPI_CHAR, 0, 99, MPI_COMM_WORLD);
	}
	else
	{
		time_cost = collect_time(*p);
		num = (double)((double)size * (double)op_time * (double)numprocs) / (double)time_cost;
		printf("num is %f\n", num);
		rate = 1000000 * num / 1024 / 1024;
		printf("Write Bandwidth = %f MB/s TimeCost = %d us\n", rate, (int)time_cost);
	}
        fclose(fp);

	file_seq += 1;
	if(file_seq == numprocs)
		file_seq = 0;
}

void read_test(int size, int op_time)
{
	char path[255];
	int i;
	double start, end, rate, num;
	int time_cost;
	char message[8];
	int *p = (int*)message;
        
	memset(buf, '\0', BUFFER_SIZE);
	memset(path, '\0', 255);
	sprintf(path, "/ssd/file_%d", file_seq);
        int fd;
        if ((fd = open(path, O_RDONLY)) == -1) {
            printf("Path %s does not exist\n", path);
            exit(-1);
        }

	MPI_Barrier ( MPI_COMM_WORLD );
	long offset = 0;
	start = MPI_Wtime();
	for(i = 0; i < op_time; i++)
	{
        	offset = (long)i * (long)size;
                read(fd, buf, size);
	}
	end = MPI_Wtime();
        close(fd);
	MPI_Barrier ( MPI_COMM_WORLD );

	*p = (int)((end - start) * 1000000);

	if(myid != 0)
	{
		MPI_Send(message, 8, MPI_CHAR, 0, 99, MPI_COMM_WORLD);
	}
	else
	{
		time_cost = collect_time(*p);
		num = (double)((double)size * (double)op_time * (double)numprocs) / (double)time_cost;
                printf("num is %f\n", num);
		rate = 1000000 * num / 1024 / 1024;
		printf("Read Bandwidth = %f MB/s TimeCost = %d us\n", rate, (int)time_cost);
	}
	MPI_Barrier ( MPI_COMM_WORLD );

	file_seq += 1;
	if(file_seq == myid)
		file_seq = 0;
}


int main(int argc, char **argv)
{

	char path[255];
	if(argc < 2)
	{
		fprintf(stderr, "Usage: ./mpibw block_size num_write IsWriteOperation\n");
                printf("Example: write test: ./mpibw 1024 10 1\n");
                printf("Example: read test: ./mpibw 1024 10 0\n");
		return -1;
	}
	int block_size = atoi(argv[1]);
	int op_time = atoi(argv[2]);
        int writeOp = atoi(argv[3]);
	MPI_Init( &argc, &argv);
	MPI_Comm_rank( MPI_COMM_WORLD, &myid );
	MPI_Comm_size( MPI_COMM_WORLD, &numprocs );
	file_seq = myid;
	MPI_Barrier ( MPI_COMM_WORLD );


	MPI_Barrier ( MPI_COMM_WORLD );

        if (writeOp == 1) {
 	  write_test(1024 * block_size, op_time);
        } else if (writeOp == 0) {
  	  read_test(1024 * block_size, op_time);
        }

	MPI_Barrier ( MPI_COMM_WORLD );
	sprintf(path, "/file_%d", myid);

	MPI_Finalize();
}
