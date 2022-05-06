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


typedef struct {
  char port_name[MPI_MAX_PORT_NAME];
  pthread_t thread;
  MPI_Comm inter_comm;
} Client;


void* serverThreadFn(void *v) {
  Client *client = (Client*) v;

  // make sure the main thread has time to start MPI_Comm_accept again
  sleep(1);

  int value = 42;
  fprintf(stderr, "Manager sending value %d over intercomm...\n", value);
  MPI_Send( &value, 1, MPI_INT,
            /* to rank zero of worker comm */ 0,0,client->inter_comm );
  fprintf(stderr, "Manager sent value %d over client->inter_comm\n", value);

  MPI_Recv(&value, 1, MPI_INT, 0, 0, client->inter_comm, MPI_STATUS_IGNORE);
  fprintf(stderr, "Manager received value %d over client->inter_comm\n", value);

  MPI_Comm_disconnect(&client->inter_comm);

  MPI_Close_port(client->port_name);

  free(client);

  return 0;
}


int main(int argc,char **argv) {

  int rlevel= MPI_THREAD_MULTIPLE, tlevel;
  MPI_Init_thread(&argc,&argv,rlevel,&tlevel);
  MPI_Comm
    comm_world = MPI_COMM_WORLD,
    comm_self = MPI_COMM_SELF;
  double start_time;

  int world_p,world_n;
  MPI_Comm_size(comm_world,&world_n);
  MPI_Comm_rank(comm_world,&world_p);
  if (tlevel!=rlevel && world_p==0) {
    printf("No thread multiple support; only %d\n",tlevel);
    MPI_Abort(comm_world,0);
  }

  start_time = MPI_Wtime();

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

    /*
    MPI_Comm intercomm;
    char myport[MPI_MAX_PORT_NAME];
    MPI_Open_port( MPI_INFO_NULL,myport );
    int portlen = strlen(myport);
    MPI_Send( myport,portlen+1,MPI_CHAR,1,0,comm_world );
    printf("Host sent port <<%s>>\n",myport);
    */

    int client_rank = 0;

    while (1) {
      Client *client = (Client*) malloc(sizeof(Client));

      MPI_Open_port( MPI_INFO_NULL, client->port_name );
      int portlen = strlen(client->port_name);
      MPI_Send( client->port_name,portlen+1,MPI_CHAR,1,0,comm_world );
      printf("Host sent port <<%s>>\n",client->port_name);

      fprintf(stderr, "host waiting for connection...\n");
      MPI_Comm_accept( client->port_name,MPI_INFO_NULL,0,comm_self,&client->inter_comm );
      fprintf(stderr, "\nhost accepted connection at %.3fs\n", MPI_Wtime() - start_time);

    /*
     * After the workers have accept the connection,
     * we can talk over the inter communicator
     */
    
      // serverThreadFn(&intercomm);

      pthread_create(&client->thread, 0, serverThreadFn, client);

      // this works fine if we wait for serverThreadFn to finish, but the thread
      // hangs in its first call to MPI_Send if we detach the thread
      // pthread_join(client->thread, 0);
      pthread_detach(client->thread);
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
    // MPI_Close_port(myport);

  } else {

    int work_p,work_n;
    MPI_Comm_size( comm_work,&work_n );
    MPI_Comm_rank( comm_work,&work_p );
    int iter = -1;
    char myport[MPI_MAX_PORT_NAME], next_port[MPI_MAX_PORT_NAME];
    MPI_Request port_name_rq;

    // start the receive for the port name
    if (work_p==0)
      MPI_Irecv(next_port, MPI_MAX_PORT_NAME, MPI_CHAR, MPI_ANY_SOURCE, 0, comm_world, &port_name_rq);

    while (1) {
      iter++;

      /*
       * In the workers communicator, rank 0
       * (which is 1 in the global)
       * receives the port name and passes it on.
       */
      if (work_p==0) {
        // wait for the port name message
        MPI_Wait(&port_name_rq, MPI_STATUS_IGNORE);
        strcpy(myport, next_port);
        // and start the receive for the next port name
        MPI_Irecv(next_port, MPI_MAX_PORT_NAME, MPI_CHAR, MPI_ANY_SOURCE, 0, comm_world, &port_name_rq);
        printf("Iteration %d, worker received port <<%s>>\n", iter, myport);
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

      sleep(10);
    }
  }

  MPI_Finalize();

  return 0;
}
