#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <math.h>

int main(int argc, char *argv[]) {

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

    if (m > n) m = n;   // 防止 m > n

    int rounds = ceil(log2(n));

    int size = sizeof(int)*(2*n + 2);

    int shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        exit(1);
    }

    void *base = shmat(shmid, NULL, 0);

    int *old = (int *)base;
    int *new = old + n;
    int *counter = new + n;
    int *sense = counter + 1;

    *counter = 0;
    *sense = 0;

    // 读输入文件
    FILE *fp = fopen(argv[3], "r");
    if (!fp) {
        perror("File open failed");
        exit(1);
    }

    for (int i = 0; i < n; i++) {
        if (fscanf(fp, "%d", &old[i]) != 1) {
            printf("Input file does not contain enough elements\n");
            exit(1);
        }
    }

    fclose(fp);

    // fork workers
    for (int i = 0; i < m; i++) {

        pid_t pid = fork();

        if (pid == 0) {

            int id = i;
            int start = id * (n / m);
            int end   = (id == m-1) ? n : (id+1)*(n/m);

            int local_sense = 0;

            for (int p = 1; p <= rounds; p++) {

                int offset = 1 << (p-1);

                for (int j = start; j < end; j++) {

                    if (j >= offset)
                        new[j] = old[j] + old[j-offset];
                    else
                        new[j] = old[j];
                }

                // -------- barrier --------
                local_sense = 1 - local_sense;

                if (__sync_fetch_and_add(counter, 1) == m-1) {
                    *counter = 0;
                    *sense = local_sense;
                } else {
                    while (*sense != local_sense);
                }

                // swap arrays
                int *tmp = old;
                old = new;
                new = tmp;
            }

            shmdt(base);
            exit(0);
        }
    }

    // 等待所有子进程
    for (int i = 0; i < m; i++)
        wait(NULL);

    // 🔥 修复 swap 奇偶问题
    // 如果 rounds 是奇数，最终结果在 new
    if (rounds % 2 != 0) {
        int *tmp = old;
        old = new;
        new = tmp;
    }

    // 写输出文件
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