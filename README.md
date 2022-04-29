# mpi_port_multi
Test case for using MPI_Open_port and MPI_THREAD_MULTIPLE

This is a test program using MPI in a server/client pattern on Frontera. It hangs when MPI_Recv() is called in one thread while another thread is blocked in a call to MPI_Comm_accept().

The MPI runtime outputs warning messages if MPI_Open_port and MPI_Comm_accept are used without setting the environment variables FI_MLX_NS_ENABLE=1 and FI_MLX_ENABLE_SPAWN=yes, so those are set in the Makefile targets `runserver` and `runclient`.

To reproduce:
1. Build: `make`
1. Start a single-node interactive job: `idev -N 1 -t 0:30:00`
1. Output the values of the environment variables `SLURM_NODELIST`, `SLURM_JOBID`, and `SLURM_NNODES`
1. Start the server: `make runserver` 
1. Using a second terminal window, log into the same node, and change to the same directory. Set `SLURM_NODELIST`, `SLURM_JOBID`, and `SLURM_NNODES` to the values saved earlier.
1. Run the client: `make runclient`

If successful, the server will output the message `[client-thread] Successful test.`  Right now it hangs in a call to MPI_Recv(), and the last output is `[client-thread] waiting for message...`.

It works on a single machine using MPICH. It also works if the call to pthread_detach() is changed to pthread_join(), because this prevents the main thread and client threads from making overlapping MPI calls.
