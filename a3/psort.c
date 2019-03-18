#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "helper.h"

/* Takes in the number of child process, number of recs in input file and a 
   int array, modify the array to contain number of recs each child process 
   would take. */
void divide_task(int num_process, int num_recs, int *split_tasks){
    int num_task = num_recs / num_process; // will never take in num_process <= 0
    int left_over = num_recs - num_task * num_process;
    // evenly split up the work
    for (int i = 0; i < left_over; i++) {
        split_tasks[i] = num_task + 1;
    }
    for (int j = left_over; j < num_process; j++) {
        split_tasks[j] = num_task;
    }
}

/* Takes in the index of first rec for current child process, number of recs to work 
   on, file name of the rec file and sorted array to be sorted. */
void do_task(int index_recs, int num_recs, char *infile, struct rec *sorted_array) {
    FILE *fp;
    // for error checking
    int r, s;
    fp = fopen(infile, "rb");
    if (fp == NULL) {
        perror("fopen");
        exit(1);
    }
    // finds the start of first rec to read
    if ((r = fseek(fp, index_recs * sizeof(struct rec), SEEK_SET)) != 0) {
        perror("fseek");
        exit(1);
    }
    // reads in recs into array
    if ((s = fread(sorted_array, sizeof(struct rec), num_recs, fp)) != num_recs) {
        perror("fread");
        exit(1);
    }
    // sorts the given array with qsort()
    qsort(sorted_array, num_recs, sizeof(struct rec), compare_freq);
    if (fclose(fp) != 0) {
        perror("fclose");
        exit(1);
    }
}

/* A merging function that takes in the result_array to be filled up with elements read in from fd pipe
   there are in total num_process number of fd pipes with num_recs to be merged. */
void merging(struct rec *result_array, int fd[][2], int *num_process, int *num_recs) {
    int byte_read, num_recs_read = 0;
    // set up an empty rec
    struct rec empty;
    empty.freq = -1;
    struct rec *merge_array = malloc(sizeof(struct rec) * *num_process);
    if (merge_array == NULL) {
        perror("malloc");
        exit(1);
    }
    // load up the inital merge array.
    for (int i = 0; i < *num_process; i++) {
        if ((byte_read = read(fd[i][0], &(merge_array[i]), sizeof(struct rec))) < 0){
            perror("read1");
            exit(1);
        }
    }
    while (num_recs_read != *num_recs) { 
        struct rec min;
        int idx_child;
        // find next min in merge array.
        if (merge_array[0].freq == -1) {
            for (int i = 1; i < *num_process; i++) {
                if (merge_array[i].freq != -1) {
                    min = merge_array[i];
                    idx_child = i;
                    break;
                }
            }
        } else {
            min = merge_array[0];
            idx_child = 0;
        }
        // finds min freq from merge array, does not read the empty rec
        for (int i = 1; i < *num_process; i++) {
            if (merge_array[i].freq != -1 && min.freq > merge_array[i].freq) {
                min = merge_array[i];
                idx_child = i;
            }
        }
        // add minimum to the result array
        result_array[num_recs_read] = min;
        // empty the array position
        merge_array[idx_child] = empty;
        // read new rec from last minimum pipe
        if ((byte_read = read(fd[idx_child][0], &(merge_array[idx_child]), sizeof(struct rec))) < 0){
            perror("read2");
            exit(1);
        }
        num_recs_read++;
    }
    // close all reading pipe from parent.
    for (int i = 0; i < *num_process; i++) {
        if (close(fd[i][0]) != 0) {
            perror("close");
            exit(1);
        }
    }
    free(merge_array);
}

/* Takes in the output file name, num_recs to be read from the result_array 
   Read them all into the output file. */
void write_out(char *outfile, int *num_recs, struct rec *result_array){
    FILE *outputFile = fopen(outfile, "wb");
    if (outputFile == NULL) {
        perror("fopen");
        exit(1);
    }
    if (fwrite(result_array, sizeof(struct rec), *num_recs, outputFile) != *num_recs) {
        perror("fwrite");
        exit(1);
    }
    if (fclose(outputFile) != 0) {
        perror("fclose");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    // for reading arguments
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
                break;
            case 'f':
                infile = optarg;
                break;
            case 'o':
                outfile = optarg;
                break;
            default:
                fprintf(stderr, "Usage: psort -n <number of processes> -f <inputfile> -o <outputfile>\n");
                exit(1);
        }
    }
    // file size of input file.
    int file_size = get_file_size(infile);
    // number of recs in infile.
    int num_recs = file_size / sizeof(struct rec); 

    // terminate the program, since there's no point going further.
    if (num_process == 0 || file_size == 0) {
        return 0;
    }
    // more child process than num of recs, then we set num_process to num_recs
    if (num_process > num_recs) {
        num_process = num_recs;
    }
    int split_task[num_process]; // records the number of recs each child process should take.

    // calculate to split up the task evenly
    divide_task(num_process, num_recs, split_task);

    int index_of_rec = 0; // index of recs for fseek in child process
    int child_idx; // an index to label child process
    int r = 1; // for starting first fork
    struct rec *sorted_array; // sorted array for each child process
    struct rec *result_array; // final result array
    int fd[num_process][2]; // pipes between parent and children

    for (int n = 0; n < num_process; n++) {
        if (r > 0) {
            // create pipe
            if (pipe(fd[n]) == -1) {
                perror("pipe");
                exit(1); 
            }
            // forking child processes
            r = fork();
            if (r < 0) {
                perror("fork"); 
                exit(1); 
            }
            // spliting up tasks for child processes
            if (r == 0) {
                child_idx = n;
                sorted_array = malloc(sizeof(struct rec) * split_task[n]);
                if (sorted_array == NULL) {
                    perror("realloc");
                    exit(1);
                }
                // sorts array using qsort
                do_task(index_of_rec, split_task[n], infile, sorted_array);
                if (close(fd[n][0]) != 0) { // close child process's reading pipe
                    perror("close");
                    exit(1);
                }
            } else {
                if (close(fd[n][1]) != 0) { // close parent process's writing pipe
                    perror("close");
                    exit(1);
                }
            }
            index_of_rec += split_task[n];
        }
    }

    if (r > 0) {
        result_array = malloc(sizeof(struct rec) * num_recs); 
        if (result_array == NULL) {
            perror("malloc");
            exit(1);
        }
        // wait for child to finish sorting then start merging
        int stat; //used for waiting message
        for (int i = 0; i < num_process; i++) {
            if (wait(&stat) == -1) {
                perror("wait");
                exit(1);
            }
            if (WEXITSTATUS(stat) != 0) {
                fprintf(stderr, "Child terminated abnormally\n");
            }
        }
        // merges child process results
        merging(result_array, fd, &num_process, &num_recs);

    } else {
        int byte_write;
        for (int i = 0; i < split_task[child_idx]; i++) {
            if ((byte_write = write(fd[child_idx][1], &(sorted_array[i]), sizeof(struct rec))) == -1) {
                perror("write");
                exit(1);
            }
        }
        if (close(fd[child_idx][1]) != 0) {
            perror("close");
            exit(1);
        }
        free(sorted_array);
        exit(0);
    }
    // output the result
    write_out(outfile, &num_recs, result_array);
    free(result_array);
    return(0);
}