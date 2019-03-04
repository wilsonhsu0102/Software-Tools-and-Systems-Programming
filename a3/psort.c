#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "helper.h"

// void excess() {
//          infp = fopen(infile, "rb");
//     if (infp == NULL) {
//         perror("fopen");
//         exit(1);
//     }
    

//     // finds the number of recs in the file and number of recs for each process. 
//     size_file = get_file_size(infile);
//     num_recs = size_file / sizeof(struct rec);
//     recs_for_process = num_recs / num_process;

//     printf("size of file: %d\n", size_file);
//     printf("number of recs of each process: %d\n", recs_for_process);

//     // reads all the recs from the input file into allocated array.
//     struct rec *rec_array = malloc(size_file);
//     int i = 0;
//     while (fread(&curr_rec, sizeof(struct rec), 1, infp) == 1) {
//         rec_array[i] = curr_rec;
//         i++;
//         printf("frequency: %d, ", curr_rec.freq);
//         printf("word: %s\n", curr_rec.word);
//     }


//     // split the job
//     int r = 1;
//     for (int i = 0; i < num_process; i++) {
//         if (r > 0) {
//             r = fork();
//             qsort(&(rec_array[i * recs_for_process]), recs_for_process, sizeof(struct rec), compare_freq);
//         }
//     }

//     if (r > 0) {
//         for (int j = 0; j < num_recs; j++) {
//                 printf("- sorted frequency: %d, ", rec_array[j].freq);
//                 printf("word: %s\n", rec_array[j].word);
//         }
//     }
        



//     free(rec_array);
//     fclose(infp);
// }

/* Takes in the number of child process, number of recs in input file and a 
   int array, modify the array to contain number of recs each child process 
   would take. */
void divide_task(int num_process, int num_recs, int *split_tasks){
    int num_task = num_recs / num_process;
    int left_over = num_recs - num_task * num_process;
    // printf("Before dividing: \n");
    // for (int i = 0; i < num_process; i++) {
    //     printf("process[%d]: %d\n", i, split_tasks[i]);
    // }


    for (int i = 0; i < left_over; i++) {
        split_tasks[i] = num_task + 1;
    }
    for (int j = left_over; j < num_process; j++) {
        split_tasks[j] = num_task;
    }

    // printf("After dividing: \n");
    // for (int i = 0; i < num_process; i++) {
    //     printf("process[%d]: %d\n", i, split_tasks[i]);
    // }
}

/* Takes in the starting rec for current child process, number of recs to work 
   on, file name of the rec file and pipe to parent */
void do_task(int index_recs, int num_recs, char *infile, int *pipe) {
    FILE *fp;
    fp = fopen(infile, "rb");
    int r, s;
    struct rec *array_rec = malloc(sizeof(struct rec) * num_recs);
    if (!fp) {
        perror("fopen");
        exit(1);
    }
    if ((r = fseek(fp, index_recs, SEEK_SET)) != 0) {
        perror("fseek");
        exit(1);
    }
    if ((s = fread(array_rec, sizeof(struct rec), num_recs, fp)) != num_recs) {
        perror("fread");
        exit(1);
    }

    printf("ppid: %d, Before sort:\n", getpid());
    for (int i = 0; i < num_recs; i++) {
        printf("frequency: %d, ", array_rec[i].freq);
        printf("word: %s\n", array_rec[i].word);
    }
    qsort(array_rec, num_recs, sizeof(struct rec), compare_freq);

    printf("ppid: %d, After sort:\n", getpid());
    for (int i = 0; i < num_recs; i++) {
        printf("- frequency: %d, ", array_rec[i].freq);
        printf("word: %s\n", array_rec[i].word);
    }

    fclose(fp);
}



int main(int argc, char *argv[]) {
    extern char *optarg;
    int flag;
    char *infile = NULL, *outfile = NULL;
    int num_process;
    
    if (argc != 7) {
        fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
        exit(1);
    }
    /* read in arguments */
    while ((flag = getopt(argc, argv, "n:f:o:")) != -1) {
        switch(flag) {
            case 'n':
                num_process = strtol(optarg, NULL, 10);
                printf("num_process: %d\n", num_process);
                break;
            case 'f':
                infile = optarg;
                printf("infile: %s\n", infile);
                break;
            case 'o':
                outfile = optarg;
                printf("outfile: %s\n", outfile);
                break;
            default:
                fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
                exit(1);
        }
    }

    // used for pipe
    int fd[num_process][2];
    // used for fork
    int r = 1;
    // records the number of recs each task should take.
    int split_task[num_process];

    divide_task(num_process, get_file_size(infile) / sizeof(struct rec), split_task);
    printf("After dividing: \n");
    for (int i = 0; i < num_process; i++) {
        printf("process[%d]: %d\n", i, split_task[i]);
    }

    int stat, pid;
    for (int n = 0; n < num_process; n++) {
        if (pipe(fd[n]) == -1) {
            perror("pipe");
            exit(1); 
        }
        // forking child processes
        if (r > 0) {
            // printf("created %d forks\n", n);
            r = fork();
            if (r < 0) {
                perror("fork"); 
                exit(1); 
            }
            // spliting up tasks
            int index_of_rec = 0;
            if (r == 0) {
                do_task(index_of_rec, split_task[n], infile, fd[n]);
            } else {
                pid = wait(&stat);
                printf("Child process: %d, exit status: %d \n", pid, WEXITSTATUS(stat));
            }
        }
        
    }


}