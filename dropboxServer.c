#include "dropboxUtil.h"
#include "dropboxServer.h"
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <pthread.h>   //for threading
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//declaracoes das funcoes
void *connection_handler(void *);

node_t* create(client_t* cli, node_t* next);
node_t* prepend(node_t* head, client_t* cli);
int count(node_t *head);
node_t* append(node_t* head, client_t* cli);
node_t* create_client_list(node_t* head);



//Terá que se protegido por mutex
node_t* head;

//Thread responsável pela conexão. Uma pra cada cliente conectado

int main(int argc, char *argv[])
{

  head = NULL;
  head = create_client_list(head);

/*
    char uid[] = "eae";
    struct file_info f;
    int li = 1;
    client_t* cli = NULL;
    cli = malloc(sizeof(client_t));
    cli->logged_in = 1;
    head = prepend(head,cli);

    cli = malloc(sizeof(client_t));
    cli->logged_in = 3;
    head = append(head,cli);
    printf("valor do logged in: %d\n", head->cli->logged_in);
    head = head->next;
    printf("valor do logged in: %d", head->cli->logged_in);*/

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

void create_user_folder(char *user_folder)
{

    printf("\nVerificando existência da pasta %s\n", user_folder);
    struct stat st = {0};
    if (stat(user_folder, &st) == -1)
    {
        printf("\nNão existe! Criando pasta...\n");
        mkdir(user_folder, 0777);
    }
}

void receive_file(char *file, int sock)
{
    char file_buffer[10000];
    FILE *fp;
    int n;

    n = recv(sock, file_buffer, sizeof(file_buffer), 0);
    {
        printf("\n\n Recebendo arquivo... \n\n");
        file_buffer[n] = '\0';
        fflush(stdout);
        fp = fopen(file, "w");
        fputs(file_buffer, fp);
        fclose(fp);
    }
    memset(file_buffer, 0, sizeof(file_buffer));

    printf("\n\n Arquivo recebido \n\n");
    char response[] = "done";
    send(sock, response, strlen(response)+1, 0);
}


void sync_clint_local_files(char *user_folder, int sock){
  int read_size;
  node_t *cursor = head;
  //Aguardo nome do arquivo
  char file_name[MAXNAME];
  if ((read_size = recv(sock, file_name, sizeof(file_name), 0)) < 0)
  {
      printf("Erro ao receber nome do arquivo\n");
  }
  file_name[read_size] = '\0';

  // envia acknowledge que recebeu
  char ack[] = "ack";
  send(sock, ack, strlen(ack)+1, 0);

  time_t lm;
  // recebe o last modified
 if ((read_size = recv(sock, &lm, sizeof(lm), 0)) < 0)
  {
      printf("Erro ao receber resposta\n");
  }
  lm = ntohl(lm);


  printf("Pasta do usuário >>%s<<\n", user_folder);
  while (cursor != NULL) {
    printf("Nome do arquivo recebido %s\n", file_name);
    printf("Nome do cliente atual >>%s<<\n", cursor->cli->userid);

    printf("userid: %s\n", cursor->cli->userid);
    printf("userfolder: %s\n", user_folder);



    if(strcmp(cursor->cli->userid,user_folder) == 0) {

        printf("Encontrou usuário: %s\n", user_folder);
        //Encontrado o cliente varre os arquivos dele para verificar se existe dado arquivo
        int file_i = 0;
        int has_file = 0;
        while(file_i < MAXFILES && has_file == 0) {
            if(strcmp(cursor->cli->f_info[file_i].name, file_name) == 0){
                printf("Nome do arquivo do servidor: %s\n", cursor->cli->f_info[file_i].name);
                has_file = 1;
                printf("Verificando versão mais atual...\n");

                char sync_local_command_reponse[20];
                if (lm < cursor->cli->f_info[file_i].last_modified) { // se o arquivo do servidor for mais atual
                    strcpy(sync_local_command_reponse,"updatelocal");
                    send(sock, sync_local_command_reponse, strlen(sync_local_command_reponse), 0); //Envia apenas o comando avisando o que deve ser feito
                    //TODO
                }
                else if (lm > cursor->cli->f_info[file_i].last_modified) { //se o arquivo do usuario for mais atual
                    strcpy(sync_local_command_reponse,"updateserver");
                    send(sock, sync_local_command_reponse, strlen(sync_local_command_reponse), 0); //Envia apenas o comando avisando o que deve ser feito
                    //TODO
                }


            }
            file_i++;
        }
        if(has_file == 0) {
            printf("Arquivo nao encontrado no servidor \n");
            //Avisa cliente que ele deve enviar arquivo
            char sync_local_command_reponse[20];
            strcpy(sync_local_command_reponse,"new_file");
            send(sock, sync_local_command_reponse, strlen(sync_local_command_reponse), 0); //Envia apenas o comando avisando o que deve ser feito
            //Aguardo nome do arquivo
            char file_name[MAXNAME];
            if ((read_size = recv(sock, file_name, sizeof(file_name), 0)) < 0)
            {
                printf("Erro ao receber nome do arquivo\n");
            }
            file_name[read_size] = '\0';
            receive_file(file_name, sock);

        }
        break;
    } else {
        cursor = cursor->next;
    }

  }

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
    char *message, command[20], user_folder[50];

    //Aguarda recebimento de mensagem do cliente. A primeira mensagem é pra definir quem ele é.
    if ((read_size = recv(sock, user_folder, sizeof(user_folder), 0)) < 0)
    {
        printf("Erro ao receber nome do usuario\n");
    }
    user_folder[read_size] = '\0';
    //TODO Verifica se já tem o máximo de usuarios logados com aquela conta
    //Cria pasta para usuário caso nao exista
    create_user_folder(user_folder);
    printf("\n\n Pasta encontrada/criada com sucesso \n\n");
    //Aguarda comandos e os executa
    while ((read_size = recv(sock, command, sizeof(command), 0)) > 0)
    {
        command[read_size] = '\0';
        printf("\n\n Comando recebido >>%s<<\n\n", command);
        if (strcmp(command, "download") == 0)
        {
        }
        else if (strcmp(command, "upload") == 0)
        {
            //Aguardo nome do arquivo
            char file_name[50];
            if ((read_size = recv(sock, file_name, sizeof(file_name), 0)) < 0)
            {
                printf("Erro ao receber nome do arquivo\n");
            }
            file_name[read_size] = '\0';
            receive_file(file_name, sock);
        }
        else if (strcmp(command, "sync_local") == 0)
        {
          printf("Iniciando sincronização...\n");
          sync_clint_local_files(user_folder, sock);
        }
        else if (strcmp(command, "list") == 0) {
            printf("\n\n Comando List recebido \n\n");


            //Informa os arquivos salvos no servidor

        }
        else
        {
            printf("Erro, comando %s desconhecido\n\n", command);
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

node_t* create(client_t* cli, node_t* next) {
    int i=0;
    node_t* new_node_t = (node_t*)malloc(sizeof(node_t));
    if (new_node_t == NULL) {
        printf("Erro criado um novo nodo.\n");
        exit(0);
    }
    new_node_t->cli = cli;
    new_node_t->next = next;

    return new_node_t;
}

node_t* prepend(node_t* head, client_t* cli) {
    node_t* new_node_t = create(cli, head);
    head = new_node_t;

    return head;
}

/*(*callback) (node_t* data) traverse(node_t* head, callback f) {
    node_t* cursor = head;
    while (cursor != NULL) {
        f(cursor);
        cursor = cursor->next;
    }
}*/

int count(node_t *head) {
    node_t *cursor = head;
    int c = 0;
    while (cursor != NULL) {
        c++;
        cursor = cursor->next;
    }

    return c;
}

node_t* append(node_t* head, client_t* cli) {
    //vai ate o ultimo nodo
    node_t *cursor = head;
    while (cursor->next != NULL) {
        cursor = cursor->next;
    }

    //cria um novo nodo
    node_t* new_node_t = create(cli, NULL);
    cursor->next = new_node_t;

    return head;

}

node_t* create_client_list(node_t* head) {
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_DIR) { // se eh um diretorio
                if (dir->d_name[0] != '.') { // se eh um diretorio valido
                    char name[9];
                    strncpy(name, dir->d_name, 8);
                    name[8] = '\0';

                    if (strcmp(name, "sync_dir")) { // se nao eh uma pasta pertencente ao usuario
                        client_t* cli = malloc(sizeof(client_t));
                        cli->devices[0] = 0;
                        cli->devices[1] = 0;
                        strcpy(cli->userid, dir->d_name);

                        DIR *user_d;
                        struct dirent  *user_f;

                        user_d = opendir(cli->userid);
                        if (user_d) {
                            int f_i = 0;

                            while ((user_f = readdir(user_d)) != NULL) {
                                if (user_f->d_name[0] != '.') {
                                    // separa nome e extensao de arquivo
                                    int file_name_i = 0;
                                    // preenche nome do arquivo
                                    while (user_f->d_name[file_name_i] != '.') {
                                        cli->f_info[f_i].name[file_name_i] = user_f->d_name[file_name_i];
                                        file_name_i++;
                                    }
                                    cli->f_info[f_i].name[file_name_i] = '\0';
                                    printf("");
                                    file_name_i++;

                                    //preenche extensao do arquivo
                                    int file_extension_i = 0;
                                    while (user_f->d_name[file_name_i] != '\0') {
                                        cli->f_info[f_i].extension[file_extension_i] = user_f->d_name[file_name_i];
                                        file_extension_i++;
                                        file_name_i++;
                                    }
                                    cli->f_info[f_i].extension[file_extension_i] = '\0';

                                    struct stat attr;
                                    // gera a string do path do arquivo
                                    char path[MAXNAME*2];
                                    strcpy(path,cli->userid);
                                    path[strlen(cli->userid)] = '/';
                                    strcpy(path+strlen(cli->userid)+1, user_f->d_name);

                                    stat(path, &attr);
                                    // preenche campo ultima modificacao
                                    cli->f_info[f_i].last_modified = attr.st_mtime;


                                    f_i++;
                                }
                            }

                        }
                        closedir(user_d);

                        cli->logged_in = 0;

                        // gera o nodo e o adiciona na lista
                        head = prepend(head,cli);


                        printf("Criado cliente. nome: %s. Nome do primeiro arquivo(soh para mostrar q funciona): %s Extensao: %s\n", cli->userid, cli->f_info[0].name, cli->f_info[0].extension);
                    }
                }
            }
        }
        closedir(d);
    }
    return head;
}
