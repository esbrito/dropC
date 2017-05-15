#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAXNAME 50
#define MAXFILES 10

struct file_info
{
    char name[MAXNAME];
    char extension[MAXNAME];
    char last_modified[MAXNAME];
    int size;
};

struct client
{
    int devices[2];
    char userid[MAXNAME];
    struct file_info f_info[MAXFILES];
    int logged_in;
};

