#include <stdio.h>  //for i/o functions
#include <stdlib.h> //for atoi, exit, etc
#include <unistd.h> //for fork()
#include <sys/ipc.h> //for shared memory
#include <sys/shm.h> //for shared memory
#include <sys/wait.h> //for wait
#include <math.h> //for log2 and ciel



/*components of this program:
1. Main Program to run 
2. Barrier Structure in o(1) space and reusable
3. Worker processes to compute sums in parallel
4. Parent process to write output

*/

//barrier logic, each worker sets a flag, and waits until all flags are set before moving on
void barrier(int *flags, int w, int m, int phase){
//flags is an array of size m, where each worker sets its flag to indicate completion of a phase. 
//w is the current worker, m = total # of workers, phase is round of computation 
    flags[w] = phase + 1; // signals the completion 

    int finished;
    do{

        finished = 1;
        for(int i = 0; i < m; i++){
            if(flags[i] < phase + 1){
                finished = 0;
                break;
            }
        }
    } while (!finished);
    
}
//end of barrier function


//main function
int main(int argc, char *argv[]) {
    if (argc != 5) { //takes in program name and 4 arguments | n (#elements), m(#workers), inputfile and outputfile
        printf("Usage: %s n m input.txt output.txt\n", argv[0]);
        return 1;
    }


    //converitng arguments from strings to integers
    int n = atoi(argv[1]); //# elements
    int m = atoi(argv[2]); //# workers

    if(n <= 0 || m <= 0 || m > n){ //sanity check
        printf("Invalid arguments, you must have a positive n and m, and n > m\n");
        return 1; 
    }



    char *inputFile = argv[3]; // storing inputfile name
    char *outputFile = argv[4]; //stores outputfile

    FILE *in = fopen(inputFile, "r"); //open inputfile to read
    if(!in){    //if you don't make it in, print and dip
        perror("error opening input file");
        return 1;

    }

    int shmid = shmget(IPC_PRIVATE, (2*n + 1 + m) * sizeof(int), IPC_CREAT | 0666);  //craete shared memory segment
    //holds two buffers size n, selector for current buffer, and m flags barrier flags
    int *shared = (int *) shmat(shmid, NULL, 0); //give pointer to this shared memory

    int *A = shared;  //worker 1
    int *B = shared + n; //worker 2
    int *current = shared + 2*n; //current buffer selector
    int *flags = shared + 2*n + 1; //barrier flags
    *current = 0; //current buffer to be 0, so starts at buffer a

    for(int i = 0; i < m; i++) flags[i] = 0; //initializing flags to be 0

    for (int i = 0; i < n; i++){
        if(fscanf(in, "%d", &A[i]) != 1){ //read in input file, if not enough elements, print and exit
            printf("input file does not contain enough elements\n");
            fclose(in);
            return 1;
        }
    } fclose(in);  //close the input file


    //spawn workers process
    int rounds = (int) ceil(log2(n)); //# rounfs of computation needed to compute sum
    //spawn workers                   //each rounds halfs the # of active workeers
    for(int w = 0; w < m; w++){
        pid_t pid = fork();   //create prrocess
        if(pid < 0){    //if the fork fails, exit
            perror("Forking failed");
            return 1;
        }

         //child process
        if(pid == 0){
            int chunk = n/m;    //segmenting a piece for each worker
            int remainder = n % m;  //incase n is not divisible by m, distribute rest of elements
            int start = w * chunk + (w < remainder ? w : remainder); //starting indec for w worker
            int end = start + chunk + (w < remainder ? 1 : 0); //ending index for w worker

            //looping through the rounds, each round halves w & doubles step size
            for(int p = 0; p < rounds; p++){
                int step = 1 << p;  //how far apart elements to the sum are, doubles every round
                int *source = (*current == 0) ? A : B;   //source buffer in memory
                int *destination = (*current == 0) ? B : A;   //destinartion buffer in memory



                //prefix sum
                for(int i = start; i < end; i++){
                
                    //if i > step, sum current element with element a step behind
                    if(i >= step) 
                
                        destination[i] = source[i] + source[i - step];   
                    
                    else
                        destination[i] = source[i];     //else copy the element

                }


                barrier(flags, w, m, p);     //call barrier to wait for workers
                
                if(w == 0) *current = 1 - *current;  //protect against race condition, only w0 changes barrier


                barrier(flags, w, m, p + rounds);   //barrier switch before next round


            
        }

            shmdt(shared);    //remove from shared memory
            exit(0);   //exit child process


    }
}



    //wait for workers finish
    for(int w = 0; w < m; w++) 
        wait(NULL); 


    //output file
    FILE *out = fopen(outputFile, "w");   //open outputFile
    int *answers = (*current == 0) ? A : B;    //pointer to final answer buffer

        for(int i = 0; i < n; i++){
            fprintf(out, "%d ", answers[i]);    //write to the utputFile

        }
        fprintf(out, "\n");   
        fclose(out);   //close output file

        printf("output written to Output file \n");

        shmdt(shared);    //remove from shared memory
        shmctl(shmid, IPC_RMID, NULL);    //shared memory for deletion
   





        return 0;

}
