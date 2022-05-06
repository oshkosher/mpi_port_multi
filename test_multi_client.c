/*
  When running this on Frontera, set these environment variables to enable intercommunicators
    FI_MLX_ENABLE_SPAWN=yes
    FI_MLX_NS_ENABLE=1

  FI_MLX_ENABLE_SPAWN=yes FI_MLX_NS_ENABLE=1 IBRUN_TASKS_PER_NODE=1 ibrun -n 1 -o 1 ./test_multi_client
*/    

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

const char *PORT_FILENAME = "test_multi.port_name";

int main(int argc, char **argv) {
  int rank;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (rank > 0) {
    MPI_Finalize();
    return 0;
  }

  /*
  char port_name[MPI_MAX_PORT_NAME+1];

  FILE *port_name_file = fopen(PORT_FILENAME, "r");
  if (!port_name_file) {
    printf("Failed to open %s\n", PORT_FILENAME);
    MPI_Finalize();
    return 1;
  }
  fscanf(port_name_file, "%s", port_name);
  */

  if (argc != 2) {
    printf("Error: expected port_name as a command line argument\n");
    MPI_Finalize();
    return 1;
  }
  const char *port_name = argv[1];

  printf("client started, connecting to %s\n", port_name);

  MPI_Comm inter_comm = MPI_COMM_NULL;
  MPI_Comm_connect(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &inter_comm);

  printf("connected to server\n");

  int value = 1;
  MPI_Send(&value, 1, MPI_INT, 0, 0, inter_comm);
  printf("sent 1, waiting for reply...\n");
  MPI_Recv(&value, 1, MPI_INT, 0, 0, inter_comm, MPI_STATUS_IGNORE);
  printf("received %d\n", value);

  value = 100;
  MPI_Send(&value, 1, MPI_INT, 0, 0, inter_comm);
  MPI_Recv(&value, 1, MPI_INT, 0, 0, inter_comm, MPI_STATUS_IGNORE);
  printf("sent 100, received %d\n", value);

  value = 1000;
  MPI_Send(&value, 1, MPI_INT, 0, 0, inter_comm);
  printf("sent disconnect\n");
  
  MPI_Comm_disconnect(&inter_comm);
  printf("disconnected\n");

  MPI_Finalize();

  return 0;
}
