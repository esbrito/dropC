
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
int connect_server(char *host, int port); 
void sync_client(); 
void send_file(char *file); 
void get_file(char *file);
void close_connection();