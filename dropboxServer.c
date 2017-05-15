#include "dropboxUtil.h"
#include "dropboxServer.h"
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <pthread.h>   //for threading

//Thread responsável pela conexão. Uma pra cada cliente conectado
void *connection_handler(void *);
int currentSocket;

int main(int argc, char *argv[])
{
    int socket_desc, client_sock, c, *socket_new;
    struct sockaddr_in server, client;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1)
    {
        printf("Falha ao criar socket");
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8888);

    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        //print the error message
        perror("Falha ao fazer o bind");
        return 1;
    }
    //Listen
    listen(socket_desc, 3);

    //Accept and incoming connection
    puts("Aguardando conexoes...");
    c = sizeof(struct sockaddr_in);
    while ((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c)))
    {
        puts("Realizada conexao");
        pthread_t connection_client_thread;
        socket_new = malloc(1);
        *socket_new = client_sock;

        if (pthread_create(&connection_client_thread, NULL, connection_handler, (void *)socket_new) < 0)
        {
            perror("Falha ao criar a thread");
            return 1;
        }

        //Realizado para não poder terminar antes da thread
        pthread_join(connection_client_thread, NULL);
        puts("Handler designado para o cliente");
    }

    if (client_sock < 0)
    {
        perror("Falha na conexao");
        return 1;
    }

    return 0;
}

void create_user_folder(char *userid)
{

    printf("\nVerificando existência da pasta %s\n", userid);
    struct stat st = {0};
    if (stat(userid, &st) == -1)
    {
        printf("\nNão existe! Criando pasta...\n");
        mkdir(userid, 0777);
    }
}

void receive_file(char *file)
{
    char file_buffer[10000];
    FILE *fp;
    int n;

    n = recv(currentSocket, file_buffer, sizeof(file_buffer), 0);
    {
        printf("\n\n Recebendo arquivo... \n\n");
        file_buffer[n] = '\0';
        fflush(stdout);
        fp = fopen(file, "w");
        fputs(file_buffer, fp);
        fclose(fp);
    }
    printf("\n\n Arquivo recebido. Informando cliente... \n\n");
    char upload_done[10];
    strcpy(upload_done, "done");
    send(currentSocket, upload_done, strlen(upload_done), 0); //Confirmação de que recebeu todo arquivo
}

/*
 * Controla a conexao para cada cliente. Uma thread para cada
 * */
void *connection_handler(void *socket_desc)
{
    int number_of_files = 0;
    //Descritor do socket
    int sock = *(int *)socket_desc;
    int read_size;
    char *message, command[10], username[50];

    //Aguarda recebimento de mensagem do cliente. A primeira mensagem é pra definir quem ele é.
    if ((read_size = recv(sock, username, sizeof(username), 0)) < 0)
    {
        printf("Erro ao receber nome do usuario\n");
    }
    username[read_size] = '\0';
    //TODO Verifica se já tem o máximo de usuarios logados com aquela conta

    //Cria pasta para usuário caso nao exista
    create_user_folder(username);
    printf("\n\n Pasta encontrada/criada com sucesso \n\n");
    //Aguarda comandos e os executa
    while ((read_size = recv(sock, command, sizeof(command), 0)) > 0)
    {
        command[read_size] = '\0';
        printf("\n\n Comando recebido %s\n\n", command);
        if (strcmp(command, "download") == 0)
        {
        }
        else if (strcmp(command, "upload") == 0)
        {
            //Faz a confirmação pro cliente do comando recebido
            printf("\n\n Comando Upload recebido \n\n");
            char upload_response[10];
            strcpy(upload_response, "upload");
            send(sock, upload_response, strlen(upload_response), 0); //Confirmação de que recebeu esse comando
            //Aguardo nome do arquivo
            char file_name[50];
            if ((read_size = recv(sock, file_name, sizeof(file_name), 0)) < 0)
            {
                printf("Erro ao receber nome do arquivo\n");
            }
            file_name[read_size] = '\0';
            send(sock, file_name, strlen(file_name), 0); // Envia confirmação de que recebeu o nome do arquivo
            currentSocket = sock;
            receive_file(file_name);
        }
        else
        {
            char *error = "Erro, comando inválido\0";
            write(sock, error, 2000);
        }
    }

    if (read_size == 0)
    {
        puts("Cliente desconectado\n");
        fflush(stdout);
    }
    else if (read_size == -1)
    {
        perror("Erro ao receber\n");
    }

    free(socket_desc);

    return 0;
}