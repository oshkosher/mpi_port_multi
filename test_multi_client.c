/*
  When running this on Frontera, set these environment variables to enable intercommunicators
    FI_MLX_ENABLE_SPAWN=yes
    FI_MLX_NS_ENABLE=1

  FI_MLX_ENABLE_SPAWN=yes FI_MLX_NS_ENABLE=1 IBRUN_TASKS_PER_NODE=1 ibrun -n 1 -o 1 ./test_multi_client
*/    

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <mpi.h>

const char *PORT_FILENAME = "test_multi.port_name";

int rank;
double t0;

double getTime() {
  return MPI_Wtime() - t0;
}

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  t0 = MPI_Wtime();

  if (rank > 0) {
    MPI_Finalize();
    return 0;
  }

  char port_name[MPI_MAX_PORT_NAME+1];

  FILE *port_name_file = fopen(PORT_FILENAME, "r");
  if (!port_name_file) {
    printf("Failed to open %s\n", PORT_FILENAME);
    MPI_Finalize();
    return 1;
  }
  fscanf(port_name_file, "%s", port_name);

  printf("%.6f client started, connecting to %s\n", getTime(), port_name);

  MPI_Comm inter_comm = MPI_COMM_NULL;
  MPI_Comm_connect(port_name, MPI_INFO_NULL, 0, MPI_COMM_WORLD, &inter_comm);

  printf("%.6f connected to server\n", getTime());

  int value = 1;
  MPI_Send(&value, 1, MPI_INT, 0, 0, inter_comm);
  MPI_Recv(&value, 1, MPI_INT, 0, 0, inter_comm, MPI_STATUS_IGNORE);
  printf("%.6f sent 1, received %d\n", getTime(), value);

  value = 100;
  MPI_Send(&value, 1, MPI_INT, 0, 0, inter_comm);
  MPI_Recv(&value, 1, MPI_INT, 0, 0, inter_comm, MPI_STATUS_IGNORE);
  printf("%.6f sent 100, received %d\n", getTime(), value);

  value = argc > 1 ? 1000000 : 1000;
  MPI_Send(&value, 1, MPI_INT, 0, 0, inter_comm);
  printf("%.6f sent %s\n", getTime(), value > 1000 ? "shutdown" : "disconnect");
  
  MPI_Comm_disconnect(&inter_comm);
  printf("%.6f disconnected\n", getTime());

  MPI_Finalize();

  return 0;
}
