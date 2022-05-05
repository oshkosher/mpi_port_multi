/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
   %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
   %%%%
   %%%% This program file is part of the book and course
   %%%% "Parallel Computing"
   %%%% by Victor Eijkhout, copyright 2013-2022
   %%%%
   %%%% portappmultiple.c : port stuff in a single executable
   %%%%
   %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
   %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include <unistd.h>
#include <mpi.h>


void* serverThreadFn(void *v) {
  MPI_Comm intercomm = *(MPI_Comm*)v;

  // make sure the main thread has time to start MPI_Comm_accept again
  sleep(1);

  int value = 42;
  fprintf(stderr, "Manager sending value %d over intercomm...\n", value);
  MPI_Send( &value, 1, MPI_INT,
            /* to rank zero of worker comm */ 0,0,intercomm );
  fprintf(stderr, "Manager sent value %d over intercomm\n", value);

  MPI_Recv(&value, 1, MPI_INT, 0, 0, intercomm, MPI_STATUS_IGNORE);
  fprintf(stderr, "Manager received value %d over intercomm\n", value);

  MPI_Comm_disconnect(&intercomm);

  return 0;
}


int main(int argc,char **argv) {

  int rlevel= MPI_THREAD_MULTIPLE, tlevel;
  MPI_Init_thread(&argc,&argv,rlevel,&tlevel);
  MPI_Comm
    comm_world = MPI_COMM_WORLD,
    comm_self = MPI_COMM_SELF;

  int world_p,world_n;
  MPI_Comm_size(comm_world,&world_n);
  MPI_Comm_rank(comm_world,&world_p);
  if (tlevel!=rlevel && world_p==0) {
    printf("No thread multiple support; only %d\n",tlevel);
    MPI_Abort(comm_world,0);
  }

  /*
   * Set up a communicator for all the worker ranks
   */
  MPI_Comm comm_work;
  {
    MPI_Group group_world,group_work;
    MPI_Comm_group( comm_world,&group_world );
    int manager[] = {0};
    MPI_Group_excl( group_world,1,manager,&group_work );
    MPI_Comm_create( comm_world,group_work,&comm_work );
    MPI_Group_free( &group_world ); MPI_Group_free( &group_work );
  }

  if (world_p==0) {
    /*
     * On world process zero open a port, and
     * send its name to world process 1,
     * which is zero in the worker comm.
     */
    MPI_Comm intercomm;
    char myport[MPI_MAX_PORT_NAME];
    MPI_Open_port( MPI_INFO_NULL,myport );
    int portlen = strlen(myport);
    MPI_Send( myport,portlen+1,MPI_CHAR,1,0,comm_world );
    printf("Host sent port <<%s>>\n",myport);

    while (1) {
      fprintf(stderr, "host waiting for connection...\n");
      MPI_Comm_accept( myport,MPI_INFO_NULL,0,comm_self,&intercomm );
      fprintf(stderr, "host accepted connection\n");

    /*
     * After the workers have accept the connection,
     * we can talk over the inter communicator
     */
    
      // serverThreadFn(&intercomm);

      pthread_t server_thread;
      pthread_create(&server_thread, 0, serverThreadFn, &intercomm);

      // this works fine if we wait for serverThreadFn to finish, but the thread
      // hangs in its first call to MPI_Send if we detach the thread
      // pthread_join(server_thread, 0);
      pthread_detach(server_thread);
    }

#if 0
    int work_n;
    MPI_Comm_remote_size(intercomm,&work_n);
    double work_data[work_n];
    MPI_Send( work_data,work_n,MPI_DOUBLE,
	      /* to rank zero of worker comm */ 0,0,intercomm );
    printf("Manager sent %d items over intercomm\n",work_n);
#endif

    /*
     * After we're done, close the port
     */
    MPI_Close_port(myport);

  } else {

    int work_p,work_n;
    MPI_Comm_size( comm_work,&work_n );
    MPI_Comm_rank( comm_work,&work_p );
    /*
     * In the workers communicator, rank 0
     * (which is 1 in the global)
     * receives the port name and passes it on.
     */
    char myport[MPI_MAX_PORT_NAME];
    if (work_p==0) {
      MPI_Recv( myport,MPI_MAX_PORT_NAME,MPI_CHAR, 
                MPI_ANY_SOURCE,0, comm_world,MPI_STATUS_IGNORE );
      printf("Worker received port <<%s>>\n",myport);
    }
    MPI_Bcast( myport,MPI_MAX_PORT_NAME,MPI_CHAR,0,comm_work );

    /*
     * The workers collective connect over the inter communicator
     */
    MPI_Comm intercomm;
    MPI_Comm_connect( myport,MPI_INFO_NULL,0,comm_work,&intercomm );
    if (work_p==0) {
      int manage_n;
      MPI_Comm_remote_size(intercomm,&manage_n);
      printf("%d workers connected to %d managers\n",work_n,manage_n);
    }

    /*
     * The local leader receives value from the manager
     */
    if (work_p==0) {
      int value;
      MPI_Status work_status;
      fprintf(stderr, "Worker zero waiting for value...\n");
      MPI_Recv( &value, 1, MPI_INT,
		/* from rank zero of manager comm */ 0,0,intercomm,&work_status );
      fprintf(stderr, "Worker zero received value %d from manager\n", value);
      value *= 10;
      MPI_Send(&value, 1, MPI_INT, 0, 0, intercomm);
    }
    /*
     * After we're done, close the connection
     */
    MPI_Close_port(myport);
  }

  MPI_Finalize();

  return 0;
}
