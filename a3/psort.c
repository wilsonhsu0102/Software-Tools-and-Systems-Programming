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
   on, file name of the rec file and sorted array to be modified. */
void do_task(int index_recs, int num_recs, char *infile, struct rec *sorted_array) {
    FILE *fp;
    // for error checking
    int r, s;
    fp = fopen(infile, "rb");
    if (!fp) {
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

void child_sort(int fd[][2], int *child_idx, int *num_process, int *index_of_rec, int *r, int *split_task, char *infile, struct rec *sorted_array) {
    for (int n = 0; n < *num_process; n++) {
        if (*r > 0) {
            // create pipe
            if (pipe(fd[n]) == -1) {
                perror("pipe");
                exit(1); 
            }
            // forking child processes
            *r = fork();
            if (*r < 0) {
                perror("fork"); 
                exit(1); 
            }
            // spliting up tasks and completing it
            if (*r == 0) {
                *child_idx = n;
                sorted_array = realloc(sorted_array, sizeof(struct rec) * split_task[n]); // need to be freed
                if (sorted_array == NULL) {
                    perror("realloc");
                    exit(1);
                }
                do_task(*index_of_rec, split_task[n], infile, sorted_array);
                close(fd[n][0]); // close child process's reading pipe
            } else {
                close(fd[n][1]); // close parent process's writing pipe
            }
            *index_of_rec += split_task[n];
        }
    }
}
//     merging(result_array, &fd, &num_process, &num_recs);
// void merging(struct rec *result_array, int fd[][2], int *num_process, int *num_recs) {
//     // start merging, first load initial array
//         for (int i = 0; i < num_process; i++) {
//             if ((byte_read = read(fd[i][0], &(merge_array[i]), sizeof(struct rec))) < 0){
//                 perror("read");
//                 exit(1);
//             }
//             // printf("merge array[%d]: freq: %d, word: %s\n bytes read: %d \n", i, merge_array[i].freq, merge_array[i].word, byte_read);
//         }
        
//         while (num_recs_read != num_recs) { 
//             struct rec min;
//             int idx_child;
//             // find next min in merge array.
//             if (merge_array[0].freq == -1) {
//                 for (int i = 1; i < num_process; i++) {
//                     if (merge_array[i].freq != -1) {
//                         min = merge_array[i];
//                         idx_child = i;
//                         break;
//                     }
//                 }
//             } else {
//                 min = merge_array[0];
//                 idx_child = 0;
//             }
//             // finds min freq from merge array
//             for (int i = 1; i < num_process; i++) {
//                 if (merge_array[i].freq != -1 && min.freq > merge_array[i].freq) {
//                     min = merge_array[i];
//                     idx_child = i;
//                 }
//             }
//             // add minimum to the result array
//             result_array[num_recs_read] = min;
//             // printf("result[%d]: freq: %d, word: %s\n", num_recs_read + 1, min.freq, min.word);
//             merge_array[idx_child] = empty;
//             // read new rec from last minimum pipe
//             if ((byte_read = read(fd[idx_child][0], &(merge_array[idx_child]), sizeof(struct rec))) < 0){
//                 perror("read");
//                 exit(1);
//             }
//             num_recs_read++;
//         }
//         for (int i = 0; i < num_process; i++) {
//             if (close(fd[i][0]) == -1) {
//                 perror("close");
//                 exit(1);
//             }
//         }
//         free(merge_array);
// }


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
    // number of recs in infile.
    int num_recs = file_size / sizeof(struct rec); 

    // terminate the program, since there's no point going further.
    if (num_process == 0 || file_size == 0) {
        return 0;
    }
    // more child process than num of recs called.
    if (num_process > num_recs) {
        num_process = num_recs;
    }

    FILE *infp = fopen(infile, "rb"); // testing
    struct rec curr_rec;
    while (fread(&curr_rec, sizeof(struct rec), 1, infp) == 1) {
        printf("frequency: %d, ", curr_rec.freq);
        printf("word: %s\n", curr_rec.word);
    }
    fclose(infp); // testing

    
    // records the number of recs each child process should take.
    int split_task[num_process];
    divide_task(num_process, num_recs, split_task);
    printf("After dividing: \n"); // testing
    for (int i = 0; i < num_process; i++) {
        printf("process[%d]: %d\n", i, split_task[i]);
    } // testing

    int stat, pid;
    // index of recs for fseek in child process and index of child process
    int index_of_rec = 0, child_idx;
    // for starting first fork
    int r = 1;
    // sorted array for each child process
    struct rec *sorted_array = malloc(sizeof(struct rec) * split_task[0]);

    // used for pipe between parent and each child process
    int fd[num_process][2];

    child_sort(fd, &child_idx, &num_process, &index_of_rec, &r, split_task, infile, sorted_array);
    // for (int n = 0; n < num_process; n++) {
    //     if (r > 0) {
    //         // create pipe
    //         if (pipe(fd[n]) == -1) {
    //             perror("pipe");
    //             exit(1); 
    //         }
    //         // forking child processes
    //         r = fork();
    //         if (r < 0) {
    //             perror("fork"); 
    //             exit(1); 
    //         }
    //         // spliting up tasks and completing it
    //         if (r == 0) {
    //             child_idx = n;
    //             sorted_array = malloc(sizeof(struct rec) * split_task[n]); // need to be freed
    //             do_task(index_of_rec, split_task[n], infile, sorted_array);
    //             close(fd[n][0]); // close child process's reading pipe
    //         } else {
    //             close(fd[n][1]); // close parent process's writing pipe
    //             // pid = wait(&stat);                                      // cant wait here, just for testing
    //             // printf("Child process: %d, exit status: %d \n", pid, WEXITSTATUS(stat));
    //         }
    //         index_of_rec += split_task[n];
    //     }
    // }

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

        // merging(result_array, &fd, &num_process, &num_recs);
        // start merging, first load initial array
        for (int i = 0; i < num_process; i++) {
            if ((byte_read = read(fd[i][0], &(merge_array[i]), sizeof(struct rec))) < 0){
                perror("read");
                exit(1);
            }
            // printf("merge array[%d]: freq: %d, word: %s\n bytes read: %d \n", i, merge_array[i].freq, merge_array[i].word, byte_read);
        }
        
        while (num_recs_read != num_recs) { 
            struct rec min;
            int idx_child;
            // find next min in merge array.
            if (merge_array[0].freq == -1) {
                for (int i = 1; i < num_process; i++) {
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
            // finds min freq from merge array
            for (int i = 1; i < num_process; i++) {
                if (merge_array[i].freq != -1 && min.freq > merge_array[i].freq) {
                    min = merge_array[i];
                    idx_child = i;
                }
            }
            // add minimum to the result array
            result_array[num_recs_read] = min;
            // printf("result[%d]: freq: %d, word: %s\n", num_recs_read + 1, min.freq, min.word);
            merge_array[idx_child] = empty;
            // read new rec from last minimum pipe
            if ((byte_read = read(fd[idx_child][0], &(merge_array[idx_child]), sizeof(struct rec))) < 0){
                perror("read");
                exit(1);
            }
            num_recs_read++;
        }
        for (int i = 0; i < num_process; i++) {
            if (close(fd[i][0]) == -1) {
                perror("close");
                exit(1);
            }
        }
        free(merge_array);
        
    } else {
        int byte_write;
        for (int i = 0; i < split_task[child_idx]; i++) {
            if ((byte_write = write(fd[child_idx][1], &(sorted_array[i]), sizeof(struct rec))) == -1) {
                perror("write");
                exit(1);
            }
            printf("- From child %d, Wrote freq: %d, word: %s\n ", child_idx, sorted_array[i].freq, sorted_array[i].word);
        }
        free(sorted_array);
        if (close(fd[child_idx][1]) == -1) {
            perror("close");
            exit(1);
        }
        exit(0);
    }
    
    for (int i = 0; i < num_recs; i++) {
        printf("result array[%d]: freq: %d, word: %s\n", i, result_array[i].freq, result_array[i].word);
    }
    return(0);
}