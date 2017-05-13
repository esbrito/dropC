#include "dropboxUtil.h"
#include "dropboxClient.h"

int main( int argc, char *argv[]){

    int port;
    char *userId = argv[1];
    char *ip = argv[2];
    char *portArg = argv[3];
    port = atoi(portArg);
  
    int sock;
    struct sockaddr_in server;
    char message[1000] , server_reply[2000];
     

    int n;
    char buf_recv[1000];
    char buf_send[1000];
    char *filename;
    char file_buffer[1000];
    FILE *fp;
    
    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created");
     
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET;
    server.sin_port = htons( port );
 
    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        perror("connect failed. Error");
        return 1;
    }
     
    puts("Connected\n");
     
    //keep communicating with server
    char command[50];
    char file_command_name[50];
    while(1)
    {
        printf("Operações disponíveis: \n\n");

        printf(">> upload <path/filename.ext> \n\n");
        printf(">> download <filename.ext> \n\n");
        printf(">> list \n \n");        
        printf(">> get_sync_dir \n \n");    

        printf(">> exit \n \n");    
        printf(">>");
        gets(message);
         
        //Send some data
        if( send(sock , message , strlen(message) , 0) < 0)
        {
            puts("Send failed");
            return 1;
        }
        //Recebe arquivo
        n = recv(sock , file_buffer , sizeof(file_buffer) , 0);
        {
            printf("\n\n Receiving the file content \n\n");
            file_buffer[n]='\0';
            fflush(stdout);
            fp = fopen("received_file.txt","w");
            fputs(file_buffer,fp);
            fclose(fp);
        }
        
    }
     
    close(sock);
    return 0;
}