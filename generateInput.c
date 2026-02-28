#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *f = fopen("input.dat", "wb"); // open for binary write
    if (!f) { perror("fopen"); return 1; }

    for (int i = 1; i <= 1000000; i++) {
        int x = i; // or use rand() for random numbers
        fwrite(&x, sizeof(int), 1, f);
    }

    fclose(f);
    return 0;
}