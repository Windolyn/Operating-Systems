#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <math.h>

/* Barrier using only read/write */
void barrier(int *flags, int w, int m, int phase) {
    flags[w] = phase + 1; // signal completion of this phase

    int done;
    do {
        done = 1;
        for (int i = 0; i < m; i++) {
            if (flags[i] < phase + 1) {
                done = 0;
                break;
            }
        }
    } while (!done);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s n m input.txt output.txt\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int m = atoi(argv[2]);

    if (n <= 0 || m <= 0 || m > n) {
        printf("Invalid arguments: require n > 0, m > 0, and n >= m\n");
        return 1;
    }

    char *input_file = argv[3];
    char *output_file = argv[4];

    FILE *in = fopen(input_file, "r");
    if (!in) {
        perror("Error opening input file");
        return 1;
    }

    // Shared memory: 2 buffers + current selector + m flags
    int shmid = shmget(IPC_PRIVATE, (2 * n + 1 + m) * sizeof(int), IPC_CREAT | 0666);
    int *shared = (int *) shmat(shmid, NULL, 0);

    int *A = shared;         // buffer 0
    int *B = shared + n;     // buffer 1
    int *current = shared + 2*n;  // buffer selector
    int *flags = shared + 2*n + 1; // barrier flags

    *current = 0;
    for (int i = 0; i < m; i++) flags[i] = 0;

    // read input
    for (int i = 0; i < n; i++) {
        if (fscanf(in, "%d", &A[i]) != 1) {
            printf("Input file does not contain enough elements\n");
            fclose(in);
            return 1;
        }
    }
    fclose(in);

    int rounds = (int) ceil(log2(n));

    // spawn workers
    for (int w = 0; w < m; w++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork failed"); return 1; }

        if (pid == 0) {
            int chunk = n / m;
            int remainder = n % m;
            int start = w * chunk + (w < remainder ? w : remainder);
            int size = chunk + (w < remainder ? 1 : 0);
            int end = start + size;

            for (int p = 0; p < rounds; p++) {
                int step = 1 << p;
                int *src = (*current == 0) ? A : B;
                int *dst = (*current == 0) ? B : A;

                // compute prefix sum for this chunk
                for (int i = start; i < end; i++) {
                    if (i >= step)
                        dst[i] = src[i] + src[i - step];
                    else
                        dst[i] = src[i];
                }

                // barrier: wait for all processes
                barrier(flags, w, m, p);

                // only worker 0 flips buffer
                if (w == 0) *current = 1 - *current;

                // barrier again to make sure buffer is flipped before next round
                barrier(flags, w, m, p + rounds);
            }

            shmdt(shared);
            exit(0);
        }
    }

    // parent waits for all children
    for (int i = 0; i < m; i++) wait(NULL);

    // write output
    FILE *out = fopen(output_file, "w");
    int *result = (*current == 0) ? A : B;
    for (int i = 0; i < n; i++) fprintf(out, "%d ", result[i]);
    fprintf(out, "\n");
    fclose(out);

    // cleanup
    shmdt(shared);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}