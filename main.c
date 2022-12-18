#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <limits.h>

#include "semun.h"
#include "defines.h"
#include "validation.h"
#include "pickers.h"
#include "segment.h"

// "${OUTPUT_PATH}" scaffold.txt 10 100
// <FILE> <CHILDREN> <LOOPS> <SEGMENTATION_FACTOR>
// X K N

int64_t timespecDiff(struct timespec *timeA_p, struct timespec *timeB_p) {
    return ((timeA_p->tv_sec * 1000000000) + timeA_p->tv_nsec) -
            ((timeB_p->tv_sec * 1000000000) + timeB_p->tv_nsec);
}

void writeToLog(int c, char * what) {
    static FILE * fp = NULL;
    const char * END = "- LOG END -\n";

    if (fp == NULL) {
        char buffer[PATH_MAX];
        sprintf(buffer, "log_c%d.txt", c);
        fp = fopen(buffer, "wt+");
    }

    if (what != NULL) {
        fputs(what, fp);
    }

    if (what == NULL && fp != NULL) {
        fputs(END, fp);
        fflush(fp);
        fclose(fp);
        fp = NULL;
    }
}

void main_child(int c, int shmid_to_parent, int shm_id_from_parent, int semid[2], int K, int N, int L, int SF) {
    char * shared_memory_to_parent;
    char * shared_memory_from_parent;
    struct sembuf sb;
    char linebuf[MAX_LINE_SIZE];
    struct timespec start, end;
    char msg[100000] = "uninitialized";

    srand(c);

    printf("Child (%d) started, lines: %d, SF: %d \n", getpid(), L, SF);

    shared_memory_to_parent = shmat(shmid_to_parent, (void *) 0, 0);
    if (shared_memory_to_parent == (void *) - 1) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }

    char * p_buffer = (char *) shared_memory_to_parent;
    int * p_id = (int*) (p_buffer + 0 * sizeof (int));
    int * p_segment = (int*) (p_buffer + 1 * sizeof (int));
    int * p_line = (int*) (p_buffer + 2 * sizeof (int));

    shared_memory_from_parent = shmat(shm_id_from_parent, (void *) 0, 0);
    if (shared_memory_from_parent == (void *) - 1) {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }

    char * p_buffer_from_parent = (char *) shared_memory_from_parent;


    //    char * current_buffer = p_buffer + c*BUF_SIZE;
    //
    uint64_t total = 0;

    for (int i = 0; i < N; i++) {
        printf("Child (%d) waiting for in queue to be available ...\n", getpid());

        clock_gettime(CLOCK_MONOTONIC, &start);

        // down empty / wait for buffer to become empty
        sb.sem_op = -1; // delta
        sb.sem_flg = 0;
        sb.sem_num = 0;
        if (semop(semid[0], &sb, 1) == -1) {
            printf("Child (%d) died ... #1\n", getpid());
            exit(1); /* error, check errno */
        }

        // produce
        printf("Child (%d) producing ...\n", getpid());
        int mysegment = pickSegment(L / SF);
        int myline = pickLine(SF);
        *p_id = c;
        *p_segment = mysegment;
        *p_line = myline;


        // up full
        sb.sem_op = 1; // delta
        sb.sem_flg = 0;
        sb.sem_num = 1;
        if (semop(semid[0], &sb, 1) == -1) {
            printf("Child (%d) died ... #2\n", getpid());
            exit(1); /* error, check errno */
        }

        clock_gettime(CLOCK_MONOTONIC, &end);

        uint64_t timeElapsed_submit = timespecDiff(&end, &start);

        // Get response:

        clock_gettime(CLOCK_MONOTONIC, &start);

        // down ci / wait for parent to place the result on memory
        sb.sem_op = -1; // delta
        sb.sem_flg = 0;
        sb.sem_num = c;

        if (semop(semid[1], &sb, 1) == -1) {
            printf("Child (%d) died ... #3\n", getpid());
            exit(1); /* error, check errno */
        }

        // consume
        strncpy(linebuf, shared_memory_from_parent, MAX_LINE_SIZE);

        clock_gettime(CLOCK_MONOTONIC, &end);

        uint64_t timeElapsed_serviced = timespecDiff(&end, &start);

        printf("Child (%d) requested line %d and received: %s\n", getpid(), myline, linebuf);

        sprintf(msg, "[%3d] R[S%4d,L%4d] submit:%10ld, serviced:%10ld, content:\"%s\"\n", c, mysegment, myline, timeElapsed_submit, timeElapsed_serviced, linebuf);
        writeToLog(c, msg);

        // send verification to the server

        // down empty / wait for buffer to become empty
        sb.sem_op = -1; // delta
        sb.sem_flg = 0;
        sb.sem_num = 0;
        if (semop(semid[0], &sb, 1) == -1) {
            printf("Child (%d) died ... #1\n", getpid());
            exit(1); /* error, check errno */
        }

        // produce
        printf("Child (%d) producing ...\n", getpid());
        *p_id = c;
        *p_segment = -1;
        *p_line = -1;


        // up full
        sb.sem_op = 1; // delta
        sb.sem_flg = 0;
        sb.sem_num = 1;
        if (semop(semid[0], &sb, 1) == -1) {
            printf("Child (%d) died ... #2\n", getpid());
            exit(1); /* error, check errno */
        }

        usleep(20000);
    }

    if (shmdt(shared_memory_to_parent) == -1) {
        fprintf(stderr, "shmdt failed\n");
        exit(EXIT_FAILURE);
    }

    if (shmdt(p_buffer_from_parent) == -1) {
        fprintf(stderr, "shmdt failed\n");
        exit(EXIT_FAILURE);
    }

    writeToLog(c, NULL);

    printf("Child (%d) calling exit(0), time elapsed: %lu ns \n", getpid(), total);
    exit(0);
}

void exit_and_fail(int semid[2], int shmids[], int N_SHMS) {
    int e = errno;
    semctl(semid[0], 0, IPC_RMID); /* clean up */
    semctl(semid[1], 0, IPC_RMID); /* clean up */

    for (int i = 0; i < N_SHMS; i++) {
        shmctl(shmids[i], IPC_RMID, 0);
    }
    errno = e;
    perror("CRITICAL: ");
    exit(e); /* error, check errno */
}

int main(int argc, char** argv) {
    char * X;
    int K, N, L = 0, SF = 0, errors = 0;
    key_t key_mem = IPC_PRIVATE;
    key_t key_sem[2] = {IPC_PRIVATE, IPC_PRIVATE};
    int SHM_SIZE_IN = 3 * sizeof (int);
    int SHM_SIZE_OUT = MAX_LINE_SIZE;
    int N_SEMS;
    int N_SHMS;
    int total_requests = 0;
    int corrupted_requests = 0;
    FILE * fp = NULL;

    int * shmids, semid[2];
    char ** shared_memories;
    Segment segment = {0};
    int * segments_read = NULL;
    int * segments_desired = NULL;
    int * linenumbers_desired = NULL;

    if (argc == 5) {
        X = argv[1];
        K = atoi(argv[2]);
        N = atoi(argv[3]);
        SF = atoi(argv[4]);
        N_SEMS = K;
        N_SHMS = K + 1;
    } else {
        X = "lorem.txt";
        K = 10;
        N = 1000;
        SF = 100;
        N_SEMS = K;
        N_SHMS = K + 1;
    }

    printf("X = %s, K (children) = %d, N (children iterations) = %d, SF (segmentation factor) = %d \n", X, K, N, SF);
    printf("Semaphores      = %d \n", N_SEMS + 2);
    printf("Memory segments = %d \n", N_SHMS);
    printf("key mem         = %d \n", key_mem);
    printf("key sem 1       = %d \n", key_sem[0]);
    printf("key sem 2       = %d \n", key_sem[1]);

    L = checkFile(X);

    printf("L               = %d (OK) \n", L);

    errors += checkNotNull(X, "Filename should be positive \n");
    errors += checkPositive(K, "Number of children should be positive \n");
    errors += checkPositive(N, "Iterations should be positive \n");
    errors += checkPositive(SF, "Segmentation factor should be positive \n");

    if (errors) {
        printf("Invalid parameters \n");
        printf("Syntax: %s <FILENAME> <CHILDREN> <ITERATIONS> <SEGMENTATION FACTOR> \n", argv[0]);
        return errors;
    } else {
        fp = fopen(X, "rt");
    }

    shmids = (int*) malloc(sizeof (int)*N_SHMS);
    shared_memories = (char**) malloc(sizeof (char*)*N_SHMS);
    segments_read = malloc(sizeof (int)*K);
    segments_desired = malloc(sizeof (int)*K);
    linenumbers_desired = malloc(sizeof (int)*K);
    segment.buffer = malloc(sizeof (char)*SF * MAX_LINE_SIZE);
    segment.loaded = false;

    for (int c = 0; c < K; c++) {
        segments_read[c] = -1;
        segments_desired[c] = -1;
        linenumbers_desired[c] = -1;
    }

    for (int i = 0; i < N_SHMS; i++) {
        const int SHM_SIZE = (i == 0) ? SHM_SIZE_IN : SHM_SIZE_OUT;

        if ((shmids[i] = shmget(key_mem, SHM_SIZE, 0644 | IPC_CREAT)) == -1) {
            perror("shmget");
            exit(1);
        }
        printf("SHM SEGMENT created with index:%3d, SHM_ID: %3d, size: %d \n", i, shmids[i], SHM_SIZE);
    }


    if ((semid[0] = semget(key_sem[0], 2, IPC_CREAT | IPC_EXCL | 0666)) == -1) { // 0:E , 1:F
        perror("semget");
        exit(1);
    }
    printf("SEM SET created with index:%3d, SEM_ID: %3d, size: %d \n", 0, semid[0], 2);

    if ((semid[1] = semget(key_sem[1], N_SEMS, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
        perror("semget");
        exit(1);
    }
    printf("SEM SET created with index:%3d, SEM_ID: %3d, size: %d \n", 1, semid[1], N_SEMS);


    struct sembuf sb;
    union semun arg;

    printf("Initializing semaphores ... \n");

    for (sb.sem_num = 0; sb.sem_num < N_SEMS; sb.sem_num++) {
        arg.val = 0;

        if (semctl(semid[1], sb.sem_num, SETVAL, arg) == -1) {
            int e = errno;
            semctl(semid[0], 0, IPC_RMID); /* clean up */
            semctl(semid[1], 0, IPC_RMID); /* clean up */

            for (int i = 0; i < N_SHMS; i++) {
                shmctl(shmids[i], IPC_RMID, 0);
            }
            errno = e;
            perror("CRITICAL: ");
            exit(e); /* error, check errno */
        }
    }

    arg.val = 1;

    if (semctl(semid[0], 0, SETVAL, arg) == -1) {
        exit_and_fail(semid, shmids, N_SHMS);
    }

    arg.val = 0;

    if (semctl(semid[0], 1, SETVAL, arg) == -1) {
        exit_and_fail(semid, shmids, N_SHMS);
    }


    pid_t * pids = malloc(K * sizeof (pid_t));

    for (int c = 0; c < K; c++) {
        pid_t temp = fork();

        if (temp == 0) {
            main_child(c, shmids[0], shmids[c + 1], semid, K, N, L, SF);
        } else if (temp < 0) {
            exit(temp);
        } else {
            pids[c] = temp;
        }
    }

    // ---------------------------- PARENT ------------------------------ /
    for (int i = 0; i < N_SHMS; i++) {
        shared_memories[i] = shmat(shmids[i], (void *) 0, 0);
        if (shared_memories == (void *) - 1) {
            fprintf(stderr, "shmat failed\n");
            exit(EXIT_FAILURE);
        }
    }

    char * p_buffer = (char *) shared_memories[0];
    int * p_id = (int*) (p_buffer + 0 * sizeof (int));
    int * p_segment = (int*) (p_buffer + 1 * sizeof (int));
    int * p_line = (int*) (p_buffer + 2 * sizeof (int));


    for (int i = 0; i < N * K; i++) {
        // down full / wait for a child to produce
        sb.sem_op = -1; // delta
        sb.sem_flg = 0;
        sb.sem_num = 1;
        if (semop(semid[0], &sb, 1) == -1) {
            exit_and_fail(semid, shmids, N_SHMS);
        }

        // cons request
        int id = *p_id;
        int segmentNumber = *p_segment;
        int lineNumber = *p_line;

        printf("Parent consumed: request: segment: %d, line: %d from process %d \n", segmentNumber, lineNumber, id);

        if (segmentNumber > (L / SF) || lineNumber >= SF || segmentNumber*lineNumber < 0) {
            corrupted_requests++;
        }

        // up empty
        sb.sem_op = 1; // delta
        sb.sem_flg = 0;
        sb.sem_num = 0;
        if (semop(semid[0], &sb, 1) == -1) {
            exit_and_fail(semid, shmids, N_SHMS);
        }

        // if verification, log that read is complete

        if (segmentNumber == -1 || lineNumber == -1) {
            segments_read[id] = -1; // read complete
            i--;

            // check if other children should be work:
            bool someone_reads_the_segment = false;

            for (int c = 0; c < K; c++) {
                if (segments_read[c] == segment.id) {
                    someone_reads_the_segment = true;
                    break;
                }
            }

            if (!someone_reads_the_segment) {
                segment.loaded = false;
                segment.id = -1;

                for (int c = 0; c < K; c++) {
                    if (segments_desired[c] != -1 && (!segment.loaded || segment.id == segments_desired[c])) {
                        segmentNumber = segments_desired[c];
                        lineNumber = linenumbers_desired[c];
                        
                        if (!segment.loaded) {
                            load(&segment, fp, segmentNumber, SF);
                        }
                        
                        copyLine(&segment, lineNumber, shared_memories[c + 1]);
                        
                        segments_read[c] = segment.id;

                        segments_desired[c] = -1;
                        linenumbers_desired[c] = -1;
                        
                        // up full of ci
                        sb.sem_op = 1; // delta
                        sb.sem_flg = 0;
                        sb.sem_num = c;

                        if (semop(semid[1], &sb, 1) == -1) {
                            exit_and_fail(semid, shmids, N_SHMS);
                        }
                    }
                }
            }

            continue;
        }
        
        total_requests++;

        if (!segment.loaded) {
            load(&segment, fp, segmentNumber, SF);
        }

        if (segment.id != segmentNumber) {
            bool someone_reads_the_segment = false;

            for (int c = 0; c < K; c++) {
                if (segments_read[c] == segment.id) {
                    someone_reads_the_segment = true;
                    break;
                }
            }

            if (someone_reads_the_segment) {
                segments_desired[id] = segmentNumber;
                linenumbers_desired[id] = lineNumber;
                continue;
            } else {
                load(&segment, fp, segmentNumber, SF);
            }
        }

        segments_read[id] = lineNumber;

        copyLine(&segment, lineNumber, shared_memories[id + 1]);

        // up full of ci
        sb.sem_op = 1; // delta
        sb.sem_flg = 0;
        sb.sem_num = id;

        if (semop(semid[1], &sb, 1) == -1) {
            exit_and_fail(semid, shmids, N_SHMS);
        }
    }


    for (int i = 0; i < N_SHMS; i++) {
        if (shmdt(shared_memories[i]) == -1) {
            fprintf(stderr, "shmdt failed\n");
            exit(EXIT_FAILURE);
        }
    }

    // ---------------------------- WAIT ------------------------------ /
    for (int i = 0; i < K; i++) {
        int status = 0;
        int j = wait(&status);

        printf("Parent: Child done.\n");
        printf("  Return value: %d\n", j);
        printf("  Status:       %d\n", status);
        printf("  WIFSTOPPED:   %d\n", WIFSTOPPED(status));
        printf("  WIFSIGNALED:  %d\n", WIFSIGNALED(status));
        printf("  WIFEXITED:    %d\n", WIFEXITED(status));
        printf("  WEXITSTATUS:  %d\n", WEXITSTATUS(status));
        printf("  WTERMSIG:     %d\n", WTERMSIG(status));
        printf("  WSTOPSIG:     %d\n", WSTOPSIG(status));
    }

    free(pids);


    for (int i = 0; i < N_SHMS; i++) {
        const int SHM_SIZE = (i == 0) ? SHM_SIZE_IN : SHM_SIZE_OUT;

        if (shmctl(shmids[i], IPC_RMID, 0) == -1) {
            fprintf(stderr, "shmctl(IPC_RMID) failed\n");
            exit(EXIT_FAILURE);
        }
        printf("SHM SEGMENT destroyed with index:%3d, SHM_ID: %3d, size: %d \n", i, shmids[i], SHM_SIZE);
    }

    if (semctl(semid[0], 0, IPC_RMID) < 0) {
        perror("Could not delete semaphore");
    }
    printf("SEM SET destroyed with index:%3d, SEM_ID: %3d, size: %d \n", 0, semid[0], 2);

    if (semctl(semid[1], 0, IPC_RMID) < 0) {
        perror("Could not delete semaphore");
    }
    printf("SEM SET destroyed with index:%3d, SEM_ID: %3d, size: %d \n", 1, semid[1], N_SEMS);


    free(shmids);
    free(shared_memories);
    free(segment.buffer);
    free(segments_read);
    free(segments_desired);
    free(linenumbers_desired);

    fclose(fp);

    printf("Total requests processed     = %d \n", total_requests);
    printf("Corrupted requests processed = %d \n", corrupted_requests);
    printf("Children                     = %d \n", K);
    printf("Iterations per child         = %d \n", N);
    printf("EXIT SUCCESS \n");

    return 0;
}

