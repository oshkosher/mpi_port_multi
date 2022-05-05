/*
  Test case using intercommunicators and multiple threads.

  When running this on Frontera, set these environment variables to enable intercommunicators
    FI_MLX_ENABLE_SPAWN=yes
    FI_MLX_NS_ENABLE=1

  FI_MLX_ENABLE_SPAWN=yes FI_MLX_NS_ENABLE=1 IBRUN_TASKS_PER_NODE=4 ibrun -n 1 ./test_multi_server
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <mpi.h>

const char *PORT_FILENAME = "test_multi.port_name";

void* connectionThreadFn(void *thread_arg) {
  MPI_Comm comm = *(MPI_Comm*) thread_arg;

  // wait for the main thead to call MPI_Comm_accept()
  sleep(1);

  printf("[client-thread] connection opened\n");

  MPI_Status status;
  int value;

  while (1) {
    printf("[client-thread] waiting for message...\n");
    MPI_Recv(&value, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &status);
    printf("[client-thread] received %d from %d\n", value, status.MPI_SOURCE);

    if (value >= 1000) break;

    value++;
    MPI_Send(&value, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, comm);
  }

  printf("[client-thread] Successful test.\n");

  if (value > 1000) {
    printf("[client-thread] shutting down...\n");
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    printf("[client-thread] shut down\n");
    exit(0);
  }
    
  printf("[client-thread] disconnecting...\n");
  MPI_Comm_disconnect(&comm);
  printf("[client-thread] disconnected\n");
  
  return 0;
}


int main(int argc, char **argv) {
  int supported_thread_level;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &supported_thread_level);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (supported_thread_level < MPI_THREAD_MULTIPLE) {
    fprintf(stderr, "[%d] Warning requested thread support level %d, got %d\n",
            rank, MPI_THREAD_MULTIPLE, supported_thread_level);
  }

  /* rank 0 is the only active process */
  if (rank > 0) {
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
  }
    
  char port_name[MPI_MAX_PORT_NAME];
  MPI_Open_port(MPI_INFO_NULL, port_name);
  printf("[main-thread] port opened: '%s'\n", port_name);

  FILE *port_file = fopen(PORT_FILENAME, "w");
  if (!port_file) {
    printf("Error: failed to open \"%s\" for writing\n", PORT_FILENAME);
  } else {
    fprintf(port_file, "%s\n", port_name);
    fclose(port_file);
    // printf("port_name written to %s\n", PORT_FILENAME);
  }

  while (1) {
    /* yes, this leaks */
    MPI_Comm *inter_comm_ptr = (MPI_Comm*) malloc(sizeof(MPI_Comm));

    printf("[main-thread] waiting for connection\n");
    MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, inter_comm_ptr);
    printf("[main-thread] connection accepted\n");

    pthread_t client_thread;
    pthread_create(&client_thread, 0, connectionThreadFn, inter_comm_ptr);

    pthread_detach(client_thread);
    // pthread_join(client_thread, 0);
  }
  
  MPI_Close_port(port_name);

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return 0;
}
