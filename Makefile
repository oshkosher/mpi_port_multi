EXEC = test_multi_server test_multi_client portappmultiple

all: $(EXEC)

MPICC = mpicc -Wall -g

test_multi_server: test_multi_server.c
	$(MPICC) -pthread $^ -o $@

test_multi_client: test_multi_client.c
	$(MPICC) $^ -o $@

portappmultiple: portappmultiple.c
	$(MPICC) $^ -o $@

runserver: test_multi_server
	FI_MLX_NS_ENABLE=1 FI_MLX_ENABLE_SPAWN=yes ibrun -n 1 ./test_multi_server

runclient: test_multi_client
	FI_MLX_NS_ENABLE=1 FI_MLX_ENABLE_SPAWN=yes ibrun -n 1 -o 1 ./test_multi_client

# mvapich2-x: MV2_SUPPORT_DPM=1

clean:
	rm -f $(EXEC) test_multi.port_name

# mpicc -Wall -O3 -march=native -mtune=native -DNDEBUG -D_GNU_SOURCE -DHAVE_LUSTRE -pthread test_multi_server.c -o test_multi_server
# mpicc -Wall -O3 -march=native -mtune=native -DNDEBUG -D_GNU_SOURCE -DHAVE_LUSTRE test_multi_client.c -o test_multi_client
