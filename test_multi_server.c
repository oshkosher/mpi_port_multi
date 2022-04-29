/*
  Test case using intercommunicators and multiple threads.

  When running this on Frontera, set these environment variables to enable intercommunicators
    FI_MLX_ENABLE_SPAWN=yes
    FI_MLX_NS_ENABLE=1

  FI_MLX_ENABLE_SPAWN=yes FI_MLX_NS_ENABLE=1 IBRUN_TASKS_PER_NODE=4 ibrun -n 1 ./inter_server_multi
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <mpi.h>

const char *PORT_FILENAME = "test_multi.port_name";

int rank, np;
double t0;

double getTime() {
  return MPI_Wtime() - t0;
}
  

void* connectionThreadFn(void *thread_arg) {
  MPI_Comm comm = *(MPI_Comm*) thread_arg;

  printf("[%x] %.6f connection opened\n", (int)comm, getTime());

  // MPI_Comm_set_errhandler(comm, MPI_ERRORS_RETURN);

  MPI_Status status;
  int value;

  while (1) {
    MPI_Recv(&value, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &status);
    printf("[%x] %.6f received %d from %d\n", (int)comm, getTime(), value, status.MPI_SOURCE);

    if (value >= 1000) break;

    value++;
    MPI_Send(&value, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, comm);
  }

  if (value > 1000) {
    printf("[%x] %.6f shutting down...\n", (int)comm, getTime());
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    printf("[%x] shut down\n", (int)comm);
    exit(0);
  }
    
  fprintf(stderr, "[%x] %.6f disconnecting...\n", (int)comm, getTime());
  MPI_Comm_disconnect(&comm);
  fprintf(stderr, "[%x] %.6f disconnected\n", *(MPI_Comm*)thread_arg, getTime());
  
  return 0;
}


int main(int argc, char **argv) {
  int supported_thread_level;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &supported_thread_level);

  MPI_Comm_size(MPI_COMM_WORLD, &np);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  t0 = MPI_Wtime();

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
  printf("%.6f port_name '%s'\n", getTime(), port_name);

  FILE *port_file = fopen(PORT_FILENAME, "w");
  if (!port_file) {
    printf("Error: failed to open \"%s\" for writing\n", PORT_FILENAME);
  } else {
    fprintf(port_file, "%s\n", port_name);
    fclose(port_file);
    printf("port_name written to %s\n", PORT_FILENAME);
  }

  while (1) {
    /* yes, this leaks */
    MPI_Comm *inter_comm_ptr = (MPI_Comm*) malloc(sizeof(MPI_Comm));

    printf("%.6f waiting for connection\n", getTime());
    MPI_Comm_accept(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, inter_comm_ptr);
    printf("%.6f connection accepted, inter_comm=%x\n",
           getTime(), (int)*inter_comm_ptr);

    pthread_t client_thread;
    pthread_create(&client_thread, 0, connectionThreadFn, inter_comm_ptr);
    pthread_detach(client_thread);
    /* pthread_join(client_thread, 0); */
  }
  
  MPI_Close_port(port_name);

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return 0;
}
