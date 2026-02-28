#include <stdio.h>  
#include <stdlib.h>
#include <sys/types.h> 
#include <unistd.h> 
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/wait.h> 
#include <math.h> 



/*components of this program:
1. Main Program to run 
    - Worker processes to compute sums in parallel
    - Parent process to write output
2. Barrier Structure in o(1) space and reusable

*/

//reuseable barrier w/ flags 
void barrier(int *flags, int w, int m, int phase){
//w is the current worker, m = total # of workers, phase is round of computation 
    flags[w] = phase + 1; 

    int finished;
    do{

        finished = 1;
        for(int i = 0; i < m; i++){
            if(flags[i] < phase + 1){
                finished = 0;
                break;
            }
        }
        if(!finished) usleep(10); 
    } while (!finished);
    
}







//main function, # of args and array of args
int main(int argc, char *argv[]) {
    if (argc != 5) { 
        printf("Usage: %s n m input.txt output.txt\n", argv[0]);
        return 1;
    }


    //converitng arguments from strings to integers
    long long n = atoi(argv[1]);
    long long m = atoi(argv[2]); 

    if(n <= 0 || m <= 0 || m > n){ 
        printf("Invalid arguments, you must have a positive n and m, and n > m\n");
        return 1; 
    }



    char *inputFile = argv[3]; 
    char *outputFile = argv[4];

    FILE *in = fopen(inputFile, "r"); 
    if(!in){    
        perror("error opening input file");
        return 1;

    }

    int shmid = shmget(IPC_PRIVATE, (2*n + 1 + m) * sizeof(long long), IPC_CREAT | 0666);  //craete shared memory 
    
    long long *shared = (long long *) shmat(shmid, NULL, 0); 

    long long *A = shared;  
    long long *B = shared + n; 
    long long *current = shared + 2*n;
    int *flags = (int *)(shared + 2*n + 1);
    *current = 0;

    for(int i = 0; i < m; i++) flags[i] = 0; 

    for (int i = 0; i < n; i++){
        if(fscanf(in, "%lld\n", &A[i]) != 1){ 
            printf("input file does not contain enough elements\n");
            fclose(in);
            return 1;
        }
    } 
    
    
    fclose(in); 




    //workers process
    int rounds = (int) ceil(log2(n)); 



    for(int w = 0; w < m; w++){
        pid_t pid = fork();   //create prrocess
        if(pid < 0){   
            perror("Forking failed");
            return 1;
        }

         //child process
        if(pid == 0){

            int chunk = n/m;  
            int remainder = n % m; 
            int start = w * chunk + (w < remainder ? w : remainder); 
            int end = start + chunk + (w < remainder ? 1 : 0); 


            for(int p = 0; p < rounds; p++){
                int step = 1 << p;  
                long long *source = (*current == 0) ? A : B;  
                long long *destination = (*current == 0) ? B : A;   



                //prefix sum
                for(int i = start; i < end; i++){
                
                   
                    if(i >= step) 
                
                        destination[i] = source[i] + source[i - step];   
                    
                    else
                

                        destination[i] = source[i];    

                }


                barrier(flags, w, m, p);     
                
            

                if(w == 0) *current = 1 - *current;  // w0 changes barrier


                barrier(flags, w, m, p + rounds);   


            
        }

            shmdt(shared);   
            exit(0);   


    }



}



    
    for(int w = 0; w < m; w++) 
        wait(NULL); 


    //output file
    FILE *out = fopen(outputFile, "w");  
    long long *answers = (*current == 0) ? A : B;    

        for(int i = 0; i < n; i++){
            fprintf(out, "%lld ", answers[i]);    //write to the utputFile

        }



        //cleaning up et closing out
        fprintf(out, "\n");   
        fclose(out);   

        printf("output written to Output file \n");

        shmdt(shared);    
        shmctl(shmid, IPC_RMID, NULL);   
   





        return 0;

}
