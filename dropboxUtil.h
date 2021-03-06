#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <utime.h>
#include <semaphore.h>



#define MAXNAME 50
#define MAXFILES 50

struct file_info
{
    char name[MAXNAME];
    char extension[MAXNAME];
    time_t last_modified;
    int size;
    int is_deleted;
    time_t deleted_date;
};

typedef struct client
{
    int devices[2];
    char userid[MAXNAME];
    struct file_info f_info[MAXFILES];
    int logged_in;
} client_t;

typedef struct node {
	struct client* cli;
	struct node* next;
} node_t;
