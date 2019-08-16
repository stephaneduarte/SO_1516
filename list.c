/*
 * list.c - implementation of the integer list functions 
 */


#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "list.h"



list_t* lst_new()
{
   list_t *list;
   list = (list_t*) malloc(sizeof(list_t));
   list->first = NULL;
   return list;
}


void lst_destroy(list_t *list)
{
	struct lst_iitem *item, *nextitem;

	item = list->first;
	while (item != NULL){
		nextitem = item->next;
		free(item);
		item = nextitem;
	}
	free(list);
}


void insert_new_process(list_t *list, int pid, time_t starttime)
{
	lst_iitem_t *item;

	item = (lst_iitem_t *) malloc (sizeof(lst_iitem_t));
	item->pid = pid;
	item->starttime = starttime;
	item->endtime = 0;
	item->next = list->first;
	list->first = item;
}


int update_terminated_process(list_t *list, int pid, int status, time_t endTime)
{
	lst_iitem_t *item;
	item = list->first;
	while(item != NULL && item->pid != pid)
		item = item->next;
	if(item != NULL){
		item->endtime = endTime;
		item->status = status;
        return difftime(item->endtime, item->starttime);
	}
    return -1;
}


void lst_print(list_t *list)
{
	lst_iitem_t *item;

	printf("Process list with status, start and end time:\n\n");
	item = list->first;
	while (item != NULL){
		if(WIFEXITED(item->status)){
            printf("PID: %d, STATUS: %d\n", item->pid, WEXITSTATUS(item->status));
            printf("Start time: %s", ctime(&(item->starttime)));
            printf("Duration: %.fs \n\n", difftime(item->endtime,  item->starttime));

		}
        else{
            printf("PID: %d, STATUS: Not terminated correctly\n", item->pid);
            printf("Start time: %s", ctime(&(item->starttime)));
            printf("Duration: %.fs \n\n", difftime(item->endtime,  item->starttime));
        }
		item = item->next;
	}
	printf("-- end of list.\n");
}
