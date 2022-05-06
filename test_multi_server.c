/*
  Test case using intercommunicators and multiple threads.

  When running this on Frontera, set these environment variables to enable intercommunicators
    FI_MLX_ENABLE_SPAWN=yes
    FI_MLX_NS_ENABLE=1

  FI_MLX_ENABLE_SPAWN=yes FI_MLX_NS_ENABLE=1 ibrun -n 1 ./test_multi_server
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <mpi.h>

const char *PORT_FILENAME = "test_multi.port_name";

typedef struct {
  int id;
  char port_name[MPI_MAX_PORT_NAME];
  pthread_t thread;
  MPI_Comm inter_comm;
} Client;


void* connectionThreadFn(void *thread_arg) {
  Client *client = (Client*) thread_arg;

  // give the main thread time to call MPI_Comm_accept()
  sleep(1);

  printf("[client-%d] connection opened\n", client->id);

  MPI_Status status;
  int value;

  while (1) {
    printf("[client-%d] waiting for message...\n", client->id);
    MPI_Recv(&value, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, client->inter_comm, &status);
    printf("[client-%d] received %d from %d\n", client->id, value, status.MPI_SOURCE);

    if (value >= 1000) break;

    value++;
    MPI_Send(&value, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, client->inter_comm);
  }

  printf("[client-%d] Successful test.\n", client->id);

  if (value > 1000) {
    printf("[client-%d] shutting down...\n", client->id);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    printf("[client-%d] shut down\n", client->id);
    exit(0);
  }
    
  printf("[client-%d] disconnecting...\n", client->id);
  MPI_Comm_disconnect(&client->inter_comm);
  printf("[client-%d] disconnected, closing port %s\n", client->id, client->port_name);

  MPI_Close_port(client->port_name);

  int save_client_id = client->id;
  free(client);

  printf("[client-%d] done\n", save_client_id);

  return 0;
}


int main(int argc, char **argv) {
  int supported_thread_level;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &supported_thread_level);

  int rank, np;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &np);

  if (np > 1) {
    fprintf(stderr, "Error: %d ranks started, this test app only supports one.\n", np);
    MPI_Finalize();
    return 1;
  }

  if (supported_thread_level < MPI_THREAD_MULTIPLE) {
    fprintf(stderr, "[%d] Warning requested thread support level %d, got %d\n",
            rank, MPI_THREAD_MULTIPLE, supported_thread_level);
  }

  Client *client = NULL;
  int next_client_id = 0;

  while (1) {
    
    client = (Client*) malloc(sizeof(Client));
    client->id = next_client_id++;
    MPI_Open_port(MPI_INFO_NULL, client->port_name);
    printf("[main-thread] port for client %d opened: '%s'\n", client->id, client->port_name);

    /*
    FILE *port_file = fopen(PORT_FILENAME, "w");
    if (!port_file) {
      printf("Error: failed to open \"%s\" for writing\n", PORT_FILENAME);
    } else {
      fprintf(port_file, "%s\n", port_name);
      fclose(port_file);
      // printf("port_name written to %s\n", PORT_FILENAME);
    }
    */

    printf("[main-thread] waiting for client %d\n", client->id);
    MPI_Comm_accept(client->port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &client->inter_comm);
    printf("[main-thread] connection %d accepted\n", client->id);

    pthread_create(&client->thread, 0, connectionThreadFn, client);

    pthread_detach(client->thread);
    // pthread_join(client->thread, 0);
  }
  
  // MPI_Close_port(port_name);

  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Finalize();
  return 0;
}
