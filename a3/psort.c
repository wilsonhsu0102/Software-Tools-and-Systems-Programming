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
void divide_task(int num_process, int num_recs, int *split_tasks){  // division error !! fix!
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

/* Takes in the index of first rec for current child process, number of recs to work 
   on, file name of the rec file and sorted array to be modified */
void do_task(int index_recs, int num_recs, char *infile, struct rec *sorted_array) {
    FILE *fp;
    // for error checking
    int r, s;
    fp = fopen(infile, "rb");
    if (!fp) {
        perror("fopen");
        exit(1);
    }
    if ((r = fseek(fp, index_recs * sizeof(struct rec), SEEK_SET)) != 0) {
        perror("fseek");
        exit(1);
    }
    if ((s = fread(sorted_array, sizeof(struct rec), num_recs, fp)) != num_recs) {
        perror("fread");
        exit(1);
    }

    // printf("ppid: %d, Before sort:\n", getpid());
    // for (int i = 0; i < num_recs; i++) {
    //     printf("frequency: %d, ", array_rec[i].freq);
    //     printf("word: %s\n", array_rec[i].word);
    // }

    qsort(sorted_array, num_recs, sizeof(struct rec), compare_freq);

    // printf("ppid: %d, After sort:\n", getpid());
    // for (int i = 0; i < num_recs; i++) {
    //     printf("- frequency: %d, ", sorted_array[i].freq);
    //     printf("word: %s\n", sorted_array[i].word);
    // }

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
    // file size of input file.
    int file_size = get_file_size(infile);

    // terminate the program, since there's no point going further.
    if (num_process == 0 || file_size == 0) {
        return 0;
    }

    FILE *infp = fopen(infile, "rb"); // testing
    struct rec curr_rec;
    while (fread(&curr_rec, sizeof(struct rec), 1, infp) == 1) {
        printf("frequency: %d, ", curr_rec.freq);
        printf("word: %s\n", curr_rec.word);
    }

    // used for pipe between parent and each child process
    int fd[num_process][2];
    // used for fork, so only parent forks.
    int r = 1;
    // records the number of recs each child process should take.
    int split_task[num_process];
    // number of recs in infile.
    int num_recs = file_size / sizeof(struct rec); 

    divide_task(num_process, num_recs, split_task);
    printf("After dividing: \n");
    for (int i = 0; i < num_process; i++) {
        printf("process[%d]: %d\n", i, split_task[i]);
    }

    int stat, pid;
    // index of recs for fseek in child process and index of child process
    int index_of_rec = 0, child_idx;
    // sorted array for each child process
    struct rec *sorted_array;
    for (int n = 0; n < num_process; n++) {
        // create pipe
        if (pipe(fd[n]) == -1) {
            perror("pipe");
            exit(1); 
        }
        // forking child processes
        if (r > 0) {
            r = fork();
            if (r < 0) {
                perror("fork"); 
                exit(1); 
            }
            // spliting up tasks and completing it
            if (r == 0) {
                child_idx = n;
                sorted_array = malloc(sizeof(struct rec) * split_task[n]);
                do_task(index_of_rec, split_task[n], infile, sorted_array);
                close(fd[n][0]); // close child process's reading pipe
            } else {
                close(fd[n][1]); // close parent process's writing pipe
                // pid = wait(&stat);                                      // testing
                // printf("Child process: %d, exit status: %d \n", pid, WEXITSTATUS(stat));
            }
            index_of_rec += split_task[n];
        }
    }

    struct rec *result_array;
    if (r > 0) {
        // empty rec to clear the array.
        struct rec empty;
        empty.freq = -1;
        struct rec *merge_array = malloc(sizeof(struct rec) * num_process); // need to be freed
        result_array = malloc(sizeof(struct rec) * num_recs); // need to be freed
        int num_recs_read = 0, byte_read;

        // wait for child to finish sorting then start merging
        for (int i = 0; i < num_process; i++) {
            pid = wait(&stat);                                      // testing 
            printf("Child process: %d, exit status: %d \n", pid, WEXITSTATUS(stat));
        }

        // start merging, first load initial array
        for (int i = 0; i < num_process; i++) {
            if ((byte_read = read(fd[i][0], &(merge_array[i]), sizeof(struct rec))) < 0){
                perror("read");
                exit(1);
            }
            // printf("merge array[%d]: freq: %d, word: %s\n bytes read: %d \n", i, merge_array[i].freq, merge_array[i].word, byte_read);
        }
        
        while (num_recs_read != num_recs) { //needs to be fixed not successfully finishing each merge. last indices
            struct rec min = merge_array[0];
            int idx_child = 0;
            // find the minimum freq in each child pipe.
            for (int i = 1; i < num_process; i++) {
                if (merge_array[i].freq != -1 && min.freq > merge_array[i].freq) {
                    min = merge_array[i];
                    idx_child = i;
                }
            }
            // add minimum to the result array
            result_array[num_recs_read] = min;
            merge_array[idx_child] = empty;
            // read new rec from last minimum pipe
            if ((byte_read = read(fd[idx_child][0], &(merge_array[idx_child]), sizeof(struct rec))) < 0){
                perror("read");
                exit(1);
            }
            printf("append[%d]: freq: %d, word: %s\n bytes read: %d \n", idx_child, merge_array[idx_child].freq, merge_array[idx_child].word, byte_read);
            num_recs_read++;
        }
        
    } else {
        int byte_write;
        for (int i = 0; i < split_task[child_idx]; i++) {
            if ((byte_write = write(fd[child_idx][1], &(sorted_array[i]), sizeof(struct rec))) == -1) {
                perror("write");
                exit(1);
            }
            // printf("- sorted_array[%d]: freq: %d, word: %s\n bytes write: %d \n", child_idx, sorted_array[child_idx].freq, sorted_array[child_idx].word, byte_write);
        }
        exit(0);
    }
    
    for (int i = 0; i < num_recs; i++) {
        printf("result array[%d]: freq: %d, word: %s\n", i, result_array[i].freq, result_array[i].word);
    }
    return(0);
}