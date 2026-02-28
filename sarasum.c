#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <math.h>

/*
Reusable barrier using flags.
Each process writes only its own flag.
No atomic operations used.
O(m) space for flags (allowed).
*/

void barrier(volatile int *flags, int id, int m, int phase)
{
    flags[id] = phase;

    int done;
    do {
        done = 1;
        for (int i = 0; i < m; i++) {
            if (flags[i] < phase) {
                done = 0;
                break;
            }
        }
    } while (!done);
}

int main(int argc, char *argv[])
{
    if (argc != 5) {
        printf("Usage: ./my-sum n m input.txt output.txt\n");
        return 1;
    }

    int n = atoi(argv[1]);
    int m = atoi(argv[2]);

    if (n <= 0 || m <= 0) {
        printf("Invalid arguments\n");
        return 1;
    }

    if (m > n) m = n;

    /* Compute number of rounds */
    int rounds = 0;
    while ((1 << rounds) < n) rounds++;

    /*
    Shared memory layout:

    old[n]
    new[n]
    flags[m]
    */

    int shmid = shmget(IPC_PRIVATE,
                       sizeof(int)*(2*n + m),
                       IPC_CREAT | 0666);

    if (shmid < 0) {
        perror("shmget failed");
        exit(1);
    }

    void *base = shmat(shmid, NULL, 0);

    int *old = (int*)base;
    int *new = old + n;
    volatile int *flags = (volatile int*)(new + n);

    /* Initialize flags */
    for (int i = 0; i < m; i++)
        flags[i] = 0;

    /* Read input */
    FILE *in = fopen(argv[3], "r");
    if (!in) {
        perror("Input open failed");
        exit(1);
    }

    for (int i = 0; i < n; i++) {
        if (fscanf(in, "%d", &old[i]) != 1) {
            printf("Input file does not contain enough elements\n");
            exit(1);
        }
    }
    fclose(in);

    /* Fork worker processes */
    for (int i = 0; i < m; i++) {

        pid_t pid = fork();

        if (pid == 0) {

            int id = i;

            int start = id * (n / m);
            int end   = (id == m-1) ? n : (id+1)*(n/m);

            for (int p = 0; p < rounds; p++) {

                int offset = 1 << p;

                for (int j = start; j < end; j++) {
                    if (j >= offset)
                        new[j] = old[j] + old[j-offset];
                    else
                        new[j] = old[j];
                }

                /* First barrier: finish computation */
                barrier(flags, id, m, 2*p + 1);

                /* Local swap */
                int *tmp = old;
                old = new;
                new = tmp;

                /* Second barrier: finish swap */
                barrier(flags, id, m, 2*p + 2);
            }

            shmdt(base);
            exit(0);
        }
    }

    /* Wait for children */
    for (int i = 0; i < m; i++)
        wait(NULL);

    /* If odd number of rounds, swap once more */
    if (rounds % 2 != 0) {
        int *tmp = old;
        old = new;
        new = tmp;
    }

    /* Write output */
    FILE *out = fopen(argv[4], "w");
    if (!out) {
        perror("Output open failed");
        exit(1);
    }

    for (int i = 0; i < n; i++)
        fprintf(out, "%d\n", old[i]);

    fclose(out);

    shmdt(base);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}