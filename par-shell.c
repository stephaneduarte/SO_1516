/*
----------------------------
SO Project
Patrick Fernandes, 81191
Stephane Duarte, 81186
Joao Oliveira, 81858
----------------------------
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>

#include "list.h"
#include "par-shell.h"
#include "pipe-io.h"

#define MAX_PAR 4
#define MAX_ARGUMENTS 5
#define MAX_LOGFILE_LINE_SIZE 100
#define MAX_STAT_MESSAGE_SIZE 100
#define MAX_OUT_FILENAME 30
#define TRUE 1
#define FALSE 0


/*GLOBAL VARIABLES, FOR INTER-THREAD COMUNICATION
-------------------------------------------------------*/
pthread_mutex_t mutex;
pthread_cond_t maxChildCond, noChildCond;
int numChildren = 0, iteration = -1, totalTime = 0;
short hasEnded = FALSE;
FILE* file;
PipeConnections terminals;
int noConnection = TRUE;
/*-----------------------------------------------------*/


/*main function of the program*/
int main(int argc, char* argv[]){
    int inputPipeFd, outputPipeFd, redirectionOutFd, dummyPipeFd=-1;
    int readStatus;
    char redirOutFilename[MAX_OUT_FILENAME];
    char tempBuffer[MAX_STAT_MESSAGE_SIZE];
    void * status;
    pthread_t monitor;
    pid_t childPid;
    list_t* listProcs;
    /*Will create a vector for the arguments of a command (plus one for the name and one for null)*/
    char* vector[MAX_ARGUMENTS + 2];

    /*-------------------------------------------*/
    /*-------------------INIT--------------------*/
    /*-------------------------------------------*/

    /*---[ Init child and terminal lists ]---*/
    listProcs = lst_new();
    terminals = newPipeConnections();


    /*---[ Signal Handling ]---*/
    signal(SIGINT, terminateBySignal);

    /*---[ Open the log file ]---*/
    if((file = fopen("log.txt", "r+")) != NULL)
        readLogFile(file);
    else
        if( (file = fopen("log.txt", "w")) ==NULL){
            fprintf(stderr, "Couldn't open/create logfile.\n");
            return EXIT_FAILURE;
        }

    /*---[ Create pipe for input ]----*/
    if( mkfifo("par-shell-in", 0666) < 0 ){
        perror("Error creating fifo pipe");
        return EXIT_FAILURE;
    }
    if( (inputPipeFd = open("par-shell-in", O_RDONLY)) == -1){
        fprintf(stderr, "Error opening fifo pipe\n");
        return EXIT_FAILURE;
    }
    noConnection=FALSE;


    /*---[ Initialize the mutex, conditional variables and the monitor thread ]---*/
    if(pthread_mutex_init(&mutex, NULL) != 0){
        perror("Error");
        return EXIT_FAILURE;
    }

    if(pthread_cond_init(&maxChildCond, NULL) != 0){
		perror("Error");
		return EXIT_FAILURE;
	}

	if(pthread_cond_init(&noChildCond, NULL) != 0){
		perror("Error");
		return EXIT_FAILURE;
	}

    if(pthread_create(&monitor, NULL, &monitorFunction, (void *) listProcs) != 0){
        fprintf(stderr, "Error creating thread\n");
        return EXIT_FAILURE;    
    }


    /*-------------------------------------------*/
    /*------------MAIN FUNCTIONALITY-------------*/
    /*-------------------------------------------*/

    while(1){

        while((readStatus = vectorFromPipe(inputPipeFd, vector))==EMPTY_PIPE){
            /*---[ Will open a dummy pipe writer to avoid active waiting ]---*/
            if( (dummyPipeFd = open("par-shell-in", O_WRONLY)) == -1){
                perror("Error opening dummy writer");
                exit(EXIT_FAILURE);
            }
        }

        /*---[  Check for a new connection  ]---*/
        if(readStatus==CONNECTION){
            addConnection(terminals, atoi(vector[0]));
            free(vector[0]);
            continue;
        }

        /*---[ Check for a disconnection ]---*/
        if(readStatus==DISCONNECTION){
            removeConnection(terminals, atoi(vector[0]));
            free(vector[0]);
            continue;
        }

        /*---[ Check for an exit message  ]---*/
        if(!strcmp(vector[0], "exit-global")){
            free(vector[0]);
            break;
        }

        /*---[ Check for a request of stats ]---*/
        if(!strcmp(vector[0], "stats")){
            if( (outputPipeFd = open(vector[1], O_WRONLY)) < 0) {
                fprintf(stderr, "Couldn't open pipe for stats\n");
                return EXIT_FAILURE;
            }
            pthread_mutex_lock(&mutex);
            sprintf(tempBuffer, "%d",numChildren);
            pthread_mutex_unlock(&mutex);
            writeToPipe(outputPipeFd, tempBuffer);
            pthread_mutex_lock(&mutex);
            sprintf(tempBuffer, "%d",totalTime);
            pthread_mutex_unlock(&mutex);
            writeToPipe(outputPipeFd, tempBuffer);
            close(outputPipeFd);
            continue;
        }

        /*---[ Check if the number of processes isn't already at it's limit ]---*/
    	pthread_mutex_lock(&mutex);
        while(numChildren >= MAX_PAR)
            pthread_cond_wait(&maxChildCond, &mutex);
        pthread_mutex_unlock(&mutex);

        /*---[ Forking ]---*/
        time_t startTime = time(NULL);
        fflush(file);
        childPid = fork();
        if(childPid == -1){
            /*If it couldn't fork, will issue the error and proceed with the program*/
            perror("Error");
        }
        else{
            if(childPid == 0){

                /*---[ Redirect output and execting the new program ]---*/
                sprintf(redirOutFilename, "par-shell-out-%ld.txt", (long int) getpid());
                if( (redirectionOutFd = open(redirOutFilename, O_RDWR | O_CREAT)) == -1){
                    fprintf(stderr, "Error redirecting output of child\n");
                    return EXIT_FAILURE;
                }
                fclose(stdout);
                dup(redirectionOutFd);
                close(redirectionOutFd);

                /*---[ Make child ignore terminal interruption ]---*/
                signal(SIGINT, SIG_IGN);

                /*---[ Execute program ]---*/
                execv(vector[0], vector);

                /*---[ Error handling ]---*/
                perror("Error");
                free(vector[0]);
                pthread_mutex_destroy(&mutex);
                lst_destroy(listProcs);
                pthread_cond_destroy(&noChildCond);
                pthread_cond_destroy(&maxChildCond);
                fclose(file);
                exit(EXIT_FAILURE);
            }
            else{
                /*---[ Add to the child process list ]---*/
                pthread_mutex_lock(&mutex);
                pthread_cond_signal(&noChildCond);
                numChildren++;
                insert_new_process(listProcs, childPid,  startTime);
                pthread_mutex_unlock(&mutex);
            }
        }
        free(vector[0]);
    }

    /*-------------------------------------------*/
    /*------------PROGRAM TERMINATION------------*/
    /*-------------------------------------------*/
    pthread_mutex_lock(&mutex);
    hasEnded = TRUE;
    /*Just in case the monitor has cleared all the processes meanwhile and entered the waiting*/
    pthread_cond_signal(&noChildCond);
    pthread_mutex_unlock(&mutex);

    sendSignalToAll(terminals, SIGUSR1);
    pthread_join(monitor, &status);
    lst_print(listProcs);
    
    /*---[ Memory Cleanup ]---*/
    lst_destroy(listProcs);
    /*Wont check for errors in the destruction since , fail or no fail, it will exit*/
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&noChildCond);
    pthread_cond_destroy(&maxChildCond);
    fclose(file);
    close(inputPipeFd);
    if(dummyPipeFd > -1) close(dummyPipeFd);
    
    if(unlink("par-shell-in")==-1){
        perror("Error deleting pipe");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

void* monitorFunction(void* l){
    list_t* list = (list_t *) l;
    int childPid, status, timeDiff;
    pthread_mutex_lock(&mutex);
    while( !hasEnded || numChildren>0){
        if(numChildren>0){
            pthread_mutex_unlock(&mutex);

            childPid = wait(&status);
            if(childPid<0){
                if (errno == EINTR) 
                    /* Este codigo de erro significa que chegou signal que interrompeu a espera 
                    pela terminacao de filho; logo voltamos a esperar */
                    continue;
                else {
                    perror("Error waiting for child.");
                    exit (EXIT_FAILURE);
                }         
            }

            pthread_mutex_lock(&mutex); /*Lock for writing acess*/
            numChildren--;
            pthread_cond_signal(&maxChildCond);
            timeDiff = update_terminated_process(list, childPid, status, time(NULL));
            fprintf(file, "iteration %d\n",++iteration);
            fprintf(file, "childPid: %d execution time: %d s\n", childPid, timeDiff);
            fprintf(file, "total execution time: %d s\n", (totalTime += timeDiff) );
        }
        else
            pthread_cond_wait(&noChildCond, &mutex);
    }
    return NULL;
}

void readLogFile(FILE* logFile){
    /*The buffer for reading a file*/
    char buffer[MAX_LOGFILE_LINE_SIZE + 1];
    char lineIter[MAX_LOGFILE_LINE_SIZE + 1];
    char lineTotalTime[MAX_LOGFILE_LINE_SIZE + 1];
    while(fgets(buffer, MAX_LOGFILE_LINE_SIZE + 1, logFile) != NULL ){
        /*THIS SECTION MUST BE CHANGED IF THE FORMAT OF THE LOG FILE IS CHANGED*/
        strcpy(lineIter, buffer);
        fgets(buffer, MAX_LOGFILE_LINE_SIZE + 1, logFile); /*Jump middle line*/
        fgets(lineTotalTime, MAX_LOGFILE_LINE_SIZE + 1, logFile);
    }
    sscanf(lineIter, "%*s %d", &iteration);
    sscanf(lineTotalTime, "%*s %*s %*s %d", &totalTime);
}

void terminateBySignal(int signum){
    /*Since the terminate by signal does exactly the same as the exit-global command
    it will just fake a pipe and send the exit-global command*/
    int signalPipe;

    /*---[ When you close the terminal without any terminal connecting ]---*/
    if(noConnection){
        if(unlink("par-shell-in")==-1) perror("Error deleting pipe");
        exit(EXIT_FAILURE);
    }

    /*---[ If at least one terminal connected while par-shell was running ]---*/
    signal(SIGINT, terminateBySignal);
    if( (signalPipe = open("par-shell-in", O_WRONLY)) == -1){
        perror("Error in signal handling");
        exit(EXIT_FAILURE);
    }
    writeToPipe(signalPipe, "exit-global");
    if(close(signalPipe)==-1){
        perror("Error in signal handling");
        exit(EXIT_FAILURE);
    }
}