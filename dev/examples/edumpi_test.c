/*
 * Testprogramm für den EduMPI-Dev-Cluster: erzeugt über einige Sekunden typische
 * Kommunikationsmuster, damit Zeitleiste, Matrix und 3D-Ansicht der GUI Daten zeigen.
 *
 *   mpicc edumpi_test.c -o edumpi_test
 *   mpirun -n 8 ./edumpi_test [Sekunden]      (Standard: 15)
 *
 * Wichtig: MPI_Init, nicht MPI_Init_thread – nur MPI_Init startet das EduMPI-Logging.
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_MSG (1 << 20) /* 1 MiB */

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double seconds = argc > 1 ? atof(argv[1]) : 15.0;
    char *buf = calloc(MAX_MSG, 1);
    char *rbuf = calloc(MAX_MSG, 1);
    char host[MPI_MAX_PROCESSOR_NAME];
    int len;
    MPI_Get_processor_name(host, &len);
    printf("Rank %d/%d auf %s\n", rank, size, host);

    /* Paare (0,1), (2,3), ...; bei ungerader Anzahl macht der letzte Rank nur Kollektive mit */
    int partner = (rank % 2 == 0) ? rank + 1 : rank - 1;
    if (partner >= size)
        partner = MPI_PROC_NULL;

    double start = MPI_Wtime();
    int iter = 0;
    while (1) {
        /* Alle Ranks entscheiden gemeinsam über das Ende, sonst hängen Kollektive */
        int done = (MPI_Wtime() - start) >= seconds;
        MPI_Bcast(&done, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (done)
            break;

        /* Blockierend: Nachrichtengrößen 8 B .. 1 MiB */
        int msg = 8 << (iter % 18);
        if (msg > MAX_MSG)
            msg = MAX_MSG;
        /* Late Sender: Rank 0 sendet jede 5. Runde verspätet, Rank 1 wartet im Recv */
        if (rank == 0 && iter % 5 == 0)
            usleep(50 * 1000);
        if (rank % 2 == 0) {
            MPI_Send(buf, msg, MPI_CHAR, partner, iter, MPI_COMM_WORLD);
            MPI_Recv(rbuf, msg, MPI_CHAR, partner, iter, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        } else {
            MPI_Recv(rbuf, msg, MPI_CHAR, partner, iter, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Send(buf, msg, MPI_CHAR, partner, iter, MPI_COMM_WORLD);
        }

        /* Nicht-blockierend im Ring */
        MPI_Request req[2];
        int right = (rank + 1) % size, left = (rank - 1 + size) % size;
        MPI_Irecv(rbuf, 4096, MPI_CHAR, left, 1000 + iter, MPI_COMM_WORLD, &req[0]);
        MPI_Isend(buf, 4096, MPI_CHAR, right, 1000 + iter, MPI_COMM_WORLD, &req[1]);
        MPI_Wait(&req[0], MPI_STATUS_IGNORE);
        MPI_Wait(&req[1], MPI_STATUS_IGNORE);

        /* Kollektive */
        MPI_Bcast(buf, 1024, MPI_CHAR, 0, MPI_COMM_WORLD);
        double local = rank + iter, sum;
        MPI_Allreduce(&local, &sum, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
        MPI_Barrier(MPI_COMM_WORLD);

        /* etwas "Rechenzeit", damit die Zeitleiste nicht nur aus MPI besteht */
        usleep(20 * 1000);
        iter++;
    }

    if (rank == 0)
        printf("Fertig nach %d Runden (%.1f s)\n", iter, MPI_Wtime() - start);

    free(buf);
    free(rbuf);
    MPI_Finalize();
    return 0;
}
