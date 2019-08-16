#ifndef PIPE_IO_H
#define PIPE_IO_H

#include <sys/types.h>

/*------------------------------------------------------------------------
Library that implements a protocol for pipe comunication for the par-shell
-------------------------------------------------------------------------*/

/*fixed size for header of a message*/
#define MESSAGE_SIZE_BSIZE 6

/*max possible size for the total of the string of a vector*/
#define TOTAL_VECTOR_MAXSIZE 500

/*codes for returns*/
#define EMPTY_PIPE 0
#define NORMAL_MESSAGE 1
#define CONNECTION 2
#define DISCONNECTION 3

/*-----Connections-----------*/
void OpenCloseConnection(int pipeFd, int operation);

/*-----Simple message------*/
int readFromPipe(int pipeFd, char* receivingBuffer);
void writeToPipe(int pipeFd, char* message);

/*-----Reading a vector with strings-------*/
void vectorToPipe(int pipeFd, char* vector[]);
int vectorFromPipe(int pipeFd, char* vector[]);


/*---------------------------------------------------------------------
The main structure for when you need to keep all the connections saved
----------------------------------------------------------------------*/
typedef struct pipeConnections_t* PipeConnections;

PipeConnections newPipeConnections();
void addConnection(PipeConnections, pid_t);
void removeConnection(PipeConnections, pid_t);
void sendSignalToAll(PipeConnections, int);
void destroyPipeConnections(PipeConnections);

#endif