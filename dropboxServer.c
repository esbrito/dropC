#include "dropboxUtil.h"
#include "dropboxServer.h"
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <pthread.h>   //for threading
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h> //for time()
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define CERTF "CertFile.pem"
#define KEYF "KeyFile.pem"

//declaracoes das funcoes
void *connection_handler(void *);
void list(char *user_folder, SSL *sock);

node_t *create(client_t *cli, node_t *next);
node_t *prepend(node_t *head, client_t *cli);
int count(node_t *head);
node_t *append(node_t *head, client_t *cli);
node_t *create_client_list(node_t *head);

node_t *head;

//Usado para proteger o acesso a estrutura de dados compartilhada entre as threads
sem_t updating, deleting, creating, logout;

int main(int argc, char *argv[])
{

    SSL_METHOD *method;
    SSL_CTX *ctx;
    OpenSSL_add_ssl_algorithms();
    SSL_load_error_strings();
    method = SSLv23_server_method();
    ctx = SSL_CTX_new(method);
    if (ctx == NULL)
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    if (SSL_CTX_use_certificate_file(ctx, CERTF, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(3);
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, KEYF, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(4);
    }

    //Inicializa semáforos como 1, fazendo eles funcionarem como mutex para garantir exclusão mútua
    sem_init(&updating, 0, 1);
    sem_init(&deleting, 0, 1);
    sem_init(&creating, 0, 1);
    sem_init(&logout, 0, 1);

    head = NULL;
    head = create_client_list(head);

    int socket_desc, client_sock, c;
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
        printf("Falha ao fazer o bind");
        return 1;
    }
    //Listen
    while (1)
    {
        listen(socket_desc, 3);

        //Aguarda conexões
        puts("Aguardando conexoes...");
        c = sizeof(struct sockaddr_in);

        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c);

        puts("Realizada conexao");
        pthread_t connection_client_thread;
        SSL *ssl_new;
        ssl_new = SSL_new(ctx);
        SSL_set_fd(ssl_new, client_sock);
        SSL_accept (ssl_new);
        if (pthread_create(&connection_client_thread, NULL, connection_handler, ssl_new) < 0)
        {
            printf("Falha ao criar a thread");
            return 1;
        }

        puts("Handler designado para o cliente");
    }

    if (client_sock < 0)
    {
        printf("Falha na conexao");
        return 1;
    }

    return 0;
}

void create_user_folder_and_update_structure(char *user_folder, SSL *sock)
{

    sem_wait(&creating);
    printf("Verificando existência da pasta %s\n", user_folder);
    struct stat st = {0};
    if (stat(user_folder, &st) == -1)
    {
        printf("Não existe! Criando pasta...\n");
        mkdir(user_folder, 0777);

        //Atualiza a estrutura de dados dos cliente e seus arquivos
        printf("Criando cliente >>%s<<\n", user_folder);
        node_t *cursor = head;
        client_t *cli = malloc(sizeof(client_t));

        printf("Alocando cliente...\n");
        //Um dos usuários está logado agora
        cli->devices[0] = 1;
        cli->devices[1] = 0;
        strcpy(cli->userid, user_folder);

        int x;
        //Inicializa nomes de arquivos com vazio
        for (x = 0; x < MAXFILES; x++)
        {
            strcpy(cli->f_info[x].name, "");
        }
        printf("Adicionando a lista...\n");
        head = append(head, cli);
        printf("Adicionado!...\n");

        //Usuário autorizado
        char response[10];
        strcpy(response, "auth");
        int datalen = strlen(response);
        int tmp = htonl(datalen);
        int n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
        if (n < 0)
            printf("Erro ao escrever no socket");
        n = SSL_write(sock, response, datalen);
        if (n < 0)
            printf("Erro ao escrever no socket");
    }
    else
    {
        //Usuário logou. Verificar se já está possui 2 logados
        node_t *cursor = head;
        //Atualiza a estrutura de dados dos cliente e seus arquivos
        printf("Atualizando estrutura de arquivos do cliente >>%s<<\n", user_folder);
        while (cursor != NULL)
        {
            if (strcmp(cursor->cli->userid, user_folder) == 0)
            {

                printf("Encontrou usuário para verificar login: %s\n", cursor->cli->userid);
                if (cursor->cli->devices[0] == 1 && cursor->cli->devices[1] == 1)
                {
                    //Avisando que não está autorizado (já tem dois ativos)
                    char response[10];
                    strcpy(response, "un_auth");
                    int datalen = strlen(response);
                    int tmp = htonl(datalen);
                    int n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
                    if (n < 0)
                        printf("Erro ao escrever no socket");
                    n = SSL_write(sock, response, datalen);
                    if (n < 0)
                        printf("Erro ao escrever no socket");
                }
                else
                {
                    char response[10];
                    strcpy(response, "auth");
                    int datalen = strlen(response);
                    int tmp = htonl(datalen);
                    int n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
                    if (n < 0)
                        printf("Erro ao escrever no socket");
                    n = SSL_write(sock, response, datalen);
                    if (n < 0)
                        printf("Erro ao escrever no socket");

                    //Seta como logado um dos dispositivos
                    if (cursor->cli->devices[0] == 0)
                    {
                        cursor->cli->devices[0] = 1;
                    }
                    else if (cursor->cli->devices[1] == 0)
                    {
                        cursor->cli->devices[1] = 1;
                    }
                }
            }
            cursor = cursor->next;
        }
    }
    sem_post(&creating);
}

void send_file(char *file, FILE *fp, SSL *sock)
{

    //Envia o nome do arquivo já com o path da pasta do servidor para facilitar. Ex. sync_dir_eduardo/arquivo.txt
    int datalen = strlen(file);
    int tmp = htonl(datalen);
    int n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket\n");
    n = SSL_write(sock, file, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket\n");

    long lSize;
    char *file_buffer;
    fseek(fp, 0L, SEEK_END);
    lSize = ftell(fp);
    rewind(fp);

    /* Aloca memória para todo o conteúdo */
    file_buffer = calloc(1, lSize + 1);
    if (!file_buffer)
        fclose(fp), fputs("Erro ao alocar", stderr), exit(1);

    /* Copia para buffer */
    if (1 != fread(file_buffer, lSize, 1, fp))
        fclose(fp), free(file_buffer), fputs("Erro na leitura", stderr), exit(1);

    printf("Enviando arquivo para o servidor....\n");
    fclose(fp);

    //Enviando de fato arquivo
    datalen = strlen(file_buffer);
    tmp = htonl(datalen);
    n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket");
    n = SSL_write(sock, file_buffer, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket");

    free(file_buffer);

    //Enviando o tempo do servidor
    char response[20];
    char time_buff[64], tmbuf[64];
    struct timeval now;
    time_t nowtime;
    struct tm *nowtm;

    int buflen;
    bzero(response, 50);
    n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
    if (n < 0)
        printf("Erro ao ler do socket");
    buflen = ntohl(buflen);
    n = SSL_read(sock, response, buflen);
    if (n < 0)
        printf("Erro ao ler do socket");

    if (strcmp(response, "getTimeServer")) {
        printf("Request de tempo do servidor recebido\n");
    } else {
      printf("\n ERROR:  >%s<", response);
    }

    //Pega o horario atual
    gettimeofday(&now, NULL);
    nowtime = now.tv_sec;
    nowtm = localtime(&nowtime);

    //Converte para imprimir
    strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S",nowtm);
    snprintf(time_buff, sizeof time_buff, "%s.%06ld",tmbuf, now.tv_usec);
    printf("\nTIME SERVER: %s\n", time_buff);

    // Altera o tempo do servidor manualmente
    // time_buff[15]++;  //Minuto
    // printf("\nTIME SERVER: %s\n", time_buff);

    datalen = strlen(time_buff);
    tmp = htonl(datalen);
    n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket");
    n = SSL_write(sock, time_buff, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket");

    //Recebendo resposta
    bzero(response,20);
    n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
    if (n < 0)
        printf("Erro ao ler do socket");
    buflen = ntohl(buflen);
    n = SSL_read(sock, response, buflen);
    if (n < 0)
        printf("Erro ao ler do socket");
    response[n] = '\0';
    printf("Tamanho: %d Mensagem: %s\n", buflen, response);
    if (strcmp(response, "done"))
    {
        printf("Arquivo recebido com sucesso!\n");
    }
}

void receive_file(char *file, SSL *sock)
{
    char file_buffer[100000];
    FILE *fp;
    int n;

    printf("Recebendo arquivo em: %s\n", file);

    //Recebe arquivo
    int buflen;
    n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
    if (n < 0)
        printf("Erro ao ler do socket");
    buflen = ntohl(buflen);
    n = SSL_read(sock, file_buffer, buflen);
    if (n < 0)
        printf("Erro ao ler do socket");
    file_buffer[n] = '\0';
    printf("Tamanho: %d Mensagem: %s\n", buflen, file_buffer);

    printf("\n\n Recebendo arquivo... \n\n");
    fflush(stdout);
    unlink(file);
    fp = fopen(file, "w");
    fputs(file_buffer, fp);
    fclose(fp);

    memset(file_buffer, 0, sizeof(file_buffer));

    printf("\n\n Arquivo recebido \n\n");
    char response[50];
    strcpy(response, "done");
    //Envia confirmacao de recebimento
    int datalen = strlen(response);
    int tmp = htonl(datalen);
    n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket");
    n = SSL_write(sock, response, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket");
}

delete_file(char *file_to_delete, char *user_folder, SSL *sock)
{

    sem_wait(&deleting);
    printf("Deletando arquivo %s do servidor", file_to_delete);
    node_t *cursor = head;
    char path_with_file[50];
    snprintf(path_with_file, sizeof(path_with_file), "%s/%s", user_folder, file_to_delete);
    //Deleta arquivo
    int ret = remove(path_with_file);
    if (ret == 0)
    {
        printf("Arquivo deletado com sucesso!");

        //Envia confirmação para o cliente
        char done[20];
        strcpy(done, "done");
        int datalen = strlen(done);
        int tmp = htonl(datalen);
        int n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
        if (n < 0)
            printf("Erro ao escrever no socket");
        n = SSL_write(sock, done, datalen);
        if (n < 0)
            printf("Erro ao escrever no socket");

        //Agora atualiza estrutura de dados

        printf("Pasta do usuário >>%s<<\n", user_folder);
        while (cursor != NULL)
        {
            printf("Nome do arquivo recebido %s\n", file_to_delete);
            printf("Nome do cliente atual >>%s<<\n", cursor->cli->userid);

            printf("userid: %s\n", cursor->cli->userid);
            printf("userfolder: %s\n", user_folder);

            if (strcmp(cursor->cli->userid, user_folder) == 0)
            {

                printf("Encontrou usuário: %s\n", user_folder);
                //Encontrado o cliente varre os arquivos dele para verificar se existe dado arquivo
                int file_i = 0;
                int has_file = 0;
                char file_name_with_extension[50];
                while (file_i < MAXFILES && has_file == 0)
                {
                    snprintf(file_name_with_extension, sizeof(file_name_with_extension), "%s.%s",
                             cursor->cli->f_info[file_i].name, cursor->cli->f_info[file_i].extension);
                    printf("Arquivo %s comparando com %s", file_name_with_extension, file_to_delete);
                    if (strcmp(file_name_with_extension, file_to_delete) == 0)
                    {
                        printf("Nome do arquivo do servidor: %s\n", cursor->cli->f_info[file_i].name);
                        cursor->cli->f_info[file_i].is_deleted = 1;
                        time(&cursor->cli->f_info[file_i].deleted_date);
                        printf("Hora de deleção do arquivo: %s\n", ctime(&(cursor->cli->f_info[file_i].deleted_date)));
                        break;
                    }
                    file_i++;
                }
                cursor = cursor->next;
            }
            else
            {
                cursor = cursor->next;
            }
        }
    }
    else
    {
        printf("Erro ao deletar arquivo");
        //Envia confirmação para o cliente
        char done[20];
        strcpy(done, "done");
        int datalen = strlen(done);
        int tmp = htonl(datalen);
        int n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
        if (n < 0)
            printf("Erro ao escrever no socket");
        n = SSL_write(sock, done, datalen);
        if (n < 0)
            printf("Erro ao escrever no socket");
    }
    sem_post(&deleting);
}

void sync_server_files(char *user_folder, SSL *sock)
{
    DIR *user_d;
    struct dirent *user_f;
    int read_size;
    // Abre pasta do cliente no servidor
    user_d = opendir(user_folder);
    if (user_d)
    {
        while ((user_f = readdir(user_d)) != NULL)
        {
            //Estrutura do arquivo sendo lido
            struct file_info current_local_file;
            if (user_f->d_name[0] != '.')
            {
                // separa nome e extensao de arquivo
                int file_name_i = 0;
                // preenche nome do arquivo
                while (user_f->d_name[file_name_i] != '.')
                {
                    current_local_file.name[file_name_i] = user_f->d_name[file_name_i];
                    file_name_i++;
                }
                current_local_file.name[file_name_i] = '\0';

                printf("Verificando arquivo: %s\n", current_local_file.name);
                file_name_i++;
                //preenche extensao do arquivo
                int file_extension_i = 0;
                while (user_f->d_name[file_name_i] != '\0')
                {
                    current_local_file.extension[file_extension_i] = user_f->d_name[file_name_i];
                    file_extension_i++;
                    file_name_i++;
                }
                current_local_file.extension[file_extension_i] = '\0';

                char file_name[MAXNAME];
                snprintf(file_name, sizeof(file_name), "sync_dir_%s/%s.%s", user_folder, current_local_file.name, current_local_file.extension);

                //Envia nome do arquivo
                int datalen = strlen(file_name);
                int tmp = htonl(datalen);
                int n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
                if (n < 0)
                    printf("Erro ao escrever no socket");
                n = SSL_write(sock, file_name, datalen);
                if (n < 0)
                    printf("Erro ao escrever no socket");

                char response[20];

                //Aguardo confirmacao de que o arquivo nao existe no cliente
                int buflen;
                n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
                if (n < 0)
                    printf("Erro ao ler do socket");
                buflen = ntohl(buflen);
                n = SSL_read(sock, response, buflen);
                if (n < 0)
                    printf("Erro ao ler do socket");
                response[n] = '\0';
                printf("Tamanho: %d Mensagem: %s\n", buflen, response);

                if (strcmp(response, "new_file") == 0)
                {
                    printf("Arquivo não encontrado no cliente, sera enviado agora \n");
                    //Envia arquivo especificado
                    char file_name_with_extension[50];
                    snprintf(file_name_with_extension, sizeof(file_name_with_extension), "%s/%s.%s", user_folder, current_local_file.name, current_local_file.extension);
                    printf("Arquivo a ser enviado: ->%s<- \n", file_name_with_extension);
                    FILE *fp;
                    printf("Verificando se arquivo existe: ->%s<- \n", file_name_with_extension);
                    if ((fp = fopen(file_name_with_extension, "r")) == NULL)
                    {
                        printf("Arquivo não encontrado\n");
                    }
                    else
                    {
                        printf("Arquivo encontrado\n");

                        int n = 0;
                        char file_name[MAXNAME];
                        snprintf(file_name, sizeof(file_name), "sync_dir_%s/%s.%s", user_folder, current_local_file.name, current_local_file.extension);
                        send_file(file_name, fp, sock);
                    }
                }
            }
        }
        //Já verificou todos arquivos

        //Envia aviso que finalizou o envio dos arquivos novos
        char done[20];
        strcpy(done, "server_done");
        int datalen = strlen(done);
        int tmp = htonl(datalen);
        int n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
        if (n < 0)
            printf("Erro ao escrever no socket");
        n = SSL_write(sock, done, datalen);
        if (n < 0)
            printf("Erro ao escrever no socket");
    }
    closedir(user_d);
}

void update_structure(char *user_folder, char *file_name, char *file_extension, time_t last_modfied)
{
    sem_wait(&updating);
    node_t *cursor = head;
    //Atualiza a estrutura de dados dos cliente e seus arquivos
    printf("Atualizando estrutura de arquivos do cliente >>%s<<\n", user_folder);
    while (cursor != NULL)
    {
        printf("Nome do arquivo recebido %s para verificar na estrutura\n", file_name);

        if (strcmp(cursor->cli->userid, user_folder) == 0)
        {

            printf("Encontrou usuário para atualizar a estrutura: %s\n", cursor->cli->userid);
            //Encontrado o cliente varre os arquivos dele para verificar se existe dado arquivo
            int file_i = 0;
            int has_file = 0;
            char file_name_with_extension[50];
            while (file_i < MAXFILES && has_file == 0)
            {
                snprintf(file_name_with_extension, sizeof(file_name_with_extension), "%s.%s",
                         cursor->cli->f_info[file_i].name, cursor->cli->f_info[file_i].extension);
                if (strcmp(file_name_with_extension, file_name) == 0)
                {
                    printf("Arquivo já existia no servidor. Atualizando dados da estrutura...\n");
                    printf("Nome do arquivo do servidor: %s\n", cursor->cli->f_info[file_i].name);
                    cursor->cli->f_info[file_i].last_modified = last_modfied; //Atualiza o valor do last modified
                    printf("Novo horário de modificação: %s\n", ctime(&(cursor->cli->f_info[file_i].last_modified)));
                    has_file = 1;
                    break;
                }
                file_i++;
            }
            //Arquivo novo no servidor, necessita adicionar
            if (has_file == 0)
            {
                printf("Arquivo é novo no servidor. Adicionar arquivo na estrutura\n");
                int i = 0;
                //Vai até a primeira posição livre do array
                printf("Iniciando busca por primeira posição livre... %s", cursor->cli->f_info[i].name);
                while (strcmp(cursor->cli->f_info[i].name, "") != 0)
                {
                    printf("Buscando primeira posição livre. Arquivo atual: %s", cursor->cli->f_info[i].name);
                    i++;
                }
                //Adiciona arquivo no array
                strcpy(cursor->cli->f_info[i].name, file_name);
                strcpy(cursor->cli->f_info[i].extension, file_extension);
                cursor->cli->f_info[i].last_modified = last_modfied;
            }
            cursor = cursor->next;
        }
        else
        {
            cursor = cursor->next;
        }
    }
    sem_post(&updating);
}

void sync_client_local_files(char *user_folder, SSL *sock)
{
    int read_size;
    node_t *cursor = head;
    char file_name[MAXNAME];

    //Aguardo nome do arquivo
    int buflen;
    int n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
    if (n < 0)
        printf("Erro ao ler do socket");
    buflen = ntohl(buflen);
    n = SSL_read(sock, file_name, buflen);
    if (n < 0)
        printf("Erro ao ler do socket");
    file_name[n] = '\0';
    printf("Tamanho: %d Mensagem: %s\n", buflen, file_name);

    time_t lm;
    //Recebe last modified
    n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
    if (n < 0)
        printf("Erro ao ler do socket");
    buflen = ntohl(buflen);
    n = SSL_read(sock, &lm, buflen);
    if (n < 0)
        printf("Erro ao ler do socket");
    lm = ntohl(lm);
    printf("Tamanho: %d Mensagem: %s\n", buflen, ctime(&lm));

    printf("Pasta do usuário >>%s<<\n", user_folder);
    while (cursor != NULL)
    {
        printf("Nome do arquivo recebido %s\n", file_name);
        printf("Nome do cliente atual >>%s<<\n", cursor->cli->userid);

        printf("userid: %s\n", cursor->cli->userid);
        printf("userfolder: %s\n", user_folder);

        if (strcmp(cursor->cli->userid, user_folder) == 0)
        {

            printf("Encontrou usuário: %s\n", user_folder);
            //Encontrado o cliente varre os arquivos dele para verificar se existe dado arquivo
            int file_i = 0;
            int has_file = 0;
            while (file_i < MAXFILES && has_file == 0)
            {
                if (strcmp(cursor->cli->f_info[file_i].name, file_name) == 0)
                {
                    printf("Nome do arquivo do servidor: %s\n", cursor->cli->f_info[file_i].name);
                    has_file = 1;
                    printf("Verificando versão mais atual...\n");
                    char sync_local_command_reponse[20];
                    if (cursor->cli->f_info[file_i].is_deleted == 1)
                    {
                        if ((lm - cursor->cli->f_info[file_i].deleted_date) < 0)
                        {
                            //Arquivo de fato foi deletado depois da ultima atualização do arquivo do cliente
                            char sync_local_command_reponse[20];

                            strcpy(sync_local_command_reponse, "delete_file");

                            //Envia apenas o comando avisando o que deve ser feito
                            int datalen = strlen(sync_local_command_reponse);
                            int tmp = htonl(datalen);
                            n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
                            if (n < 0)
                                printf("Erro ao escrever no socket");
                            n = SSL_write(sock, sync_local_command_reponse, datalen);
                            if (n < 0)
                                printf("Erro ao escrever no socket");
                        }
                        else
                        {
                            // Usuário tem arquivo mais atual que quando o antigo foi deletado. Deve atualizar
                            cursor->cli->f_info[file_i].is_deleted = 0;
                            printf("Arquivo do usuário é mais novo \n");

                            printf("Last modified cliente %s \n", ctime(&lm));
                            printf("Deleted date do servidor: %s \n", ctime(&(cursor->cli->f_info[file_i].deleted_date)));

                            char sync_local_command_reponse[20];

                            strcpy(sync_local_command_reponse, "updateserver");

                            //Envia apenas o comando avisando o que deve ser feito
                            int datalen = strlen(sync_local_command_reponse);
                            int tmp = htonl(datalen);
                            n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
                            if (n < 0)
                                printf("Erro ao escrever no socket");
                            n = SSL_write(sock, sync_local_command_reponse, datalen);
                            if (n < 0)
                                printf("Erro ao escrever no socket");

                            cursor->cli->f_info[file_i].last_modified = lm;

                            char file_name[MAXNAME];

                            //Aguardo nome do arquivo
                            int buflen;
                            int n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
                            if (n < 0)
                                printf("Erro ao ler do socket");
                            buflen = ntohl(buflen);
                            n = SSL_read(sock, file_name, buflen);
                            if (n < 0)
                                printf("Erro ao ler do socket");
                            file_name[n] = '\0';
                            printf("Tamanho: %d Mensagem: %s\n", buflen, file_name);

                            char file_extension[MAXNAME];
                            //Aguardo nome da extensão do arquivo
                            n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
                            if (n < 0)
                                printf("Erro ao ler do socket");
                            buflen = ntohl(buflen);
                            n = SSL_read(sock, file_extension, buflen);
                            if (n < 0)
                                printf("Erro ao ler do socket");
                            file_extension[n] = '\0';
                            printf("Tamanho: %d Mensagem: %s\n", buflen, file_extension);

                            char file_name_with_extension_and_path[100];
                            snprintf(file_name_with_extension_and_path, sizeof(file_name_with_extension_and_path), "%s/%s.%s",
                                     user_folder, file_name, file_extension);
                            receive_file(file_name_with_extension_and_path, sock);
                            update_structure(user_folder, file_name, file_extension, lm);
                        }
                    }
                    else if ((lm - cursor->cli->f_info[file_i].last_modified) < -5)
                    { // se o arquivo do servidor for mais atual, com um espaco de 5 segundos
                        printf("Arquivo do servidor é mais novo \n");

                        printf("Last modified cliente %s \n", ctime(&lm));
                        printf("Last modified servidor: %s \n", ctime(&(cursor->cli->f_info[file_i].last_modified)));
                        strcpy(sync_local_command_reponse, "updatelocal");

                        //Envia apenas o comando avisando o que deve ser feito
                        int datalen = strlen(sync_local_command_reponse);
                        int tmp = htonl(datalen);
                        n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
                        if (n < 0)
                            printf("Erro ao escrever no socket");
                        n = SSL_write(sock, sync_local_command_reponse, datalen);
                        if (n < 0)
                            printf("Erro ao escrever no socket");

                        FILE *fp;
                        char file_to_send[50];
                        char file_path[50];

                        snprintf(file_to_send, sizeof(file_to_send), "%s/%s.%s", user_folder, file_name, cursor->cli->f_info[file_i].extension);
                        snprintf(file_path, sizeof(file_path), "sync_dir_%s/%s.%s", user_folder, file_name, cursor->cli->f_info[file_i].extension);

                        printf("Verificando se arquivo existe: ->%s<- \n", file_name);
                        if ((fp = fopen(file_to_send, "r")) == NULL)
                        {
                            printf("Arquivo não encontrado\n");
                        }
                        else
                        {
                            printf("\nArquivo encontrado\n");
                            send_file(file_path, fp, sock);
                        }
                    }
                    else if ((lm - cursor->cli->f_info[file_i].last_modified) > 5)
                    { //se o arquivo do usuario for mais atual, com um espaco de 5 segundos
                        printf("Arquivo do usuário é mais novo \n");

                        printf("Last modified cliente %s \n", ctime(&lm));
                        printf("Last modified servidor: %s \n", ctime(&(cursor->cli->f_info[file_i].last_modified)));

                        char sync_local_command_reponse[20];

                        strcpy(sync_local_command_reponse, "updateserver");

                        //Envia apenas o comando avisando o que deve ser feito
                        int datalen = strlen(sync_local_command_reponse);
                        int tmp = htonl(datalen);
                        n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
                        if (n < 0)
                            printf("Erro ao escrever no socket");
                        n = SSL_write(sock, sync_local_command_reponse, datalen);
                        if (n < 0)
                            printf("Erro ao escrever no socket");

                        cursor->cli->f_info[file_i].last_modified = lm;

                        char file_name[MAXNAME];

                        //Aguardo nome do arquivo
                        int buflen;
                        int n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
                        if (n < 0)
                            printf("Erro ao ler do socket");
                        buflen = ntohl(buflen);
                        n = SSL_read(sock, file_name, buflen);
                        if (n < 0)
                            printf("Erro ao ler do socket");
                        file_name[n] = '\0';
                        printf("Tamanho: %d Mensagem: %s\n", buflen, file_name);

                        char file_extension[MAXNAME];
                        //Aguardo nome da extensão do arquivo
                        n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
                        if (n < 0)
                            printf("Erro ao ler do socket");
                        buflen = ntohl(buflen);
                        n = SSL_read(sock, file_extension, buflen);
                        if (n < 0)
                            printf("Erro ao ler do socket");
                        file_extension[n] = '\0';
                        printf("Tamanho: %d Mensagem: %s\n", buflen, file_extension);

                        char file_name_with_extension_and_path[100];
                        snprintf(file_name_with_extension_and_path, sizeof(file_name_with_extension_and_path), "%s/%s.%s",
                                 user_folder, file_name, file_extension);
                        receive_file(file_name_with_extension_and_path, sock);
                        update_structure(user_folder, file_name, file_extension, lm);
                    }
                    else
                    {
                        printf("Arquivos sao iguais\n");
                        strcpy(sync_local_command_reponse, "iguais");
                        //Envia apenas o comando avisando o que deve ser feito
                        int datalen = strlen(sync_local_command_reponse);
                        int tmp = htonl(datalen);
                        n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
                        if (n < 0)
                            printf("Erro ao escrever no socket");
                        n = SSL_write(sock, sync_local_command_reponse, datalen);
                        if (n < 0)
                            printf("Erro ao escrever no socket");
                    }
                }
                file_i++;
            }
            if (has_file == 0)
            {
                printf("Arquivo nao encontrado no servidor \n");
                //Avisa cliente que ele deve enviar arquivo
                char sync_local_command_reponse[20];
                strcpy(sync_local_command_reponse, "new_file");

                //Envia apenas o comando avisando o que deve ser feito
                int datalen = strlen(sync_local_command_reponse);
                int tmp = htonl(datalen);
                n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
                if (n < 0)
                    printf("Erro ao escrever no socket");
                n = SSL_write(sock, sync_local_command_reponse, datalen);
                if (n < 0)
                    printf("Erro ao escrever no socket");

                char file_name[MAXNAME];

                //Aguardo nome do arquivo
                int buflen;
                int n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
                if (n < 0)
                    printf("Erro ao ler do socket");
                buflen = ntohl(buflen);
                n = SSL_read(sock, file_name, buflen);
                if (n < 0)
                    printf("Erro ao ler do socket");
                file_name[n] = '\0';
                printf("Tamanho: %d Mensagem: %s\n", buflen, file_name);

                char file_extension[MAXNAME];
                //Aguardo nome da extensão do arquivo
                n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
                if (n < 0)
                    printf("Erro ao ler do socket");
                buflen = ntohl(buflen);
                n = SSL_read(sock, file_extension, buflen);
                if (n < 0)
                    printf("Erro ao ler do socket");
                file_extension[n] = '\0';
                printf("Tamanho: %d Mensagem: %s\n", buflen, file_extension);

                char file_name_with_extension_and_path[100];
                snprintf(file_name_with_extension_and_path, sizeof(file_name_with_extension_and_path), "%s/%s.%s",
                         user_folder, file_name, file_extension);
                receive_file(file_name_with_extension_and_path, sock);
                update_structure(user_folder, file_name, file_extension, lm);
            }
            break;
        }
        else
        {
            cursor = cursor->next;
        }
    }
}

void logout_user(char *user_folder)
{
    sem_wait(&logout);
    node_t *cursor = head;
    //Atualiza a estrutura de dados dos cliente e seus arquivos
    printf("Atualizando estrutura de arquivos do cliente >>%s<<\n", user_folder);
    while (cursor != NULL)
    {
        if (strcmp(cursor->cli->userid, user_folder) == 0)
        {

            printf("Encontrou usuário para deslogar: %s\n", cursor->cli->userid);

            //Seta como logado um dos dispositivos
            if (cursor->cli->devices[0] == 1)
            {
                printf("Usuário %s deslogado!", cursor->cli->userid);
                cursor->cli->devices[0] = 0;
            }
            else if (cursor->cli->devices[1] == 1)
            {
                printf("Usuário %s deslogado!", cursor->cli->userid);
                cursor->cli->devices[1] = 0;
            }
            break;
        }
        cursor = cursor->next;
    }
    sem_post(&logout);
}

/*
 * Controla a conexao para cada cliente. Uma thread para cada
 * */
void *connection_handler(void *socket_desc)
{
    int number_of_files = 0;
    //Descritor do socket
    SSL *sock = socket_desc;
    int read_size;
    char *message;
    char command[20];
    char user_folder[50];

    //Aguarda recebimento de mensagem do cliente. A primeira mensagem é pra definir quem ele é.
    int buflen;
    int n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
    if (n < 0)
        printf("Erro ao ler do socket");
    buflen = ntohl(buflen);
    n = SSL_read(sock, user_folder, buflen);
    if (n < 0)
        printf("Erro ao ler do socket");
    user_folder[n] = '\0';
    printf("Tamanho: %d Mensagem: %s\n", buflen, user_folder);

    //Cria pasta para usuário caso nao exista
    create_user_folder_and_update_structure(user_folder, sock);
    printf("\n\n Pasta encontrada/criada com sucesso \n\n");
    //Aguarda comandos e os executa
    int command_size;
    while (n = SSL_read(sock, (char *)&command_size, sizeof(command_size)) > 0)
    {
        if (n < 0)
            printf("Erro ao ler do socket");
        command_size = ntohl(command_size);
        n = SSL_read(sock, command, command_size);
        if (n < 0)
            printf("Erro ao ler do socket");
        command[n] = '\0';
        printf("Tamanho: %d Mensagem: %s\n", command_size, command);
        printf("\n\n Comando recebido >>%s<<\n\n", command);

        //Seleciona de acordo com comando recebido
        if (strcmp(command, "download") == 0)
        {
            char file_name_with_extension[50];
            //Aguardo nome do arquivo
            buflen;
            n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
            if (n < 0)
                printf("Erro ao ler do socket");
            buflen = ntohl(buflen);
            n = SSL_read(sock, file_name_with_extension, buflen);
            if (n < 0)
                printf("Erro ao ler do socket");
            file_name_with_extension[n] = '\0';
            printf("Tamanho: %d Mensagem: %s\n", buflen, file_name_with_extension);

            FILE *fp;
            char file_to_send[50];
            char file_path[50];

            snprintf(file_to_send, sizeof(file_to_send), "%s/%s", user_folder, file_name_with_extension);
            snprintf(file_path, sizeof(file_path), "%s", file_name_with_extension);

            printf("Verificando se arquivo existe: ->%s<- \n", file_name_with_extension);
            if ((fp = fopen(file_to_send, "r")) == NULL)
            {
                printf("Arquivo não encontrado\n");
            }
            else
            {
                printf("\nArquivo encontrado\n");
                send_file(file_path, fp, sock);
            }
        }
        else if (strcmp(command, "upload") == 0)
        {

            printf("Preparando para receber arquivo...\n");
            time_t lm;

            //Recebe last modified
            n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
            if (n < 0)
                printf("Erro ao ler do socket");
            buflen = ntohl(buflen);
            n = SSL_read(sock, &lm, buflen);
            if (n < 0)
                printf("Erro ao ler do socket");
            lm = ntohl(lm);
            printf("Tamanho: %d Mensagem: %s\n", buflen, ctime(&lm));

            printf("Data de modificação do arquivo %s\n", ctime(&lm));
            char file_name[MAXNAME];

            //Aguardo nome do arquivo
            n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
            if (n < 0)
                printf("Erro ao ler do socket");
            buflen = ntohl(buflen);
            n = SSL_read(sock, file_name, buflen);
            if (n < 0)
                printf("Erro ao ler do socket");
            file_name[n] = '\0';
            printf("Tamanho: %d Mensagem: %s\n", buflen, file_name);

            char file_extension[MAXNAME];
            //Aguardo nome da extensão do arquivo
            n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
            if (n < 0)
                printf("Erro ao ler do socket");
            buflen = ntohl(buflen);
            n = SSL_read(sock, file_extension, buflen);
            if (n < 0)
                printf("Erro ao ler do socket");
            file_extension[n] = '\0';
            printf("Tamanho: %d Mensagem: %s\n", buflen, file_extension);

            char file_name_with_extension_and_path[100];
            snprintf(file_name_with_extension_and_path, sizeof(file_name_with_extension_and_path), "%s/%s.%s",
                     user_folder, file_name, file_extension);
            printf("Recebendo arquivo %s", file_name_with_extension_and_path);
            receive_file(file_name_with_extension_and_path, sock);
            update_structure(user_folder, file_name, file_extension, lm);
        }
        else if (strcmp(command, "sync_local") == 0)
        {
            printf("Iniciando sincronização...\n");
            sync_client_local_files(user_folder, sock);
            printf("Sincronização finalizada...\n");
        }
        else if (strcmp(command, "exit") == 0)
        {
            char user[50];
            printf("Usuário deslogando\n");
            //Aguardo nome do usuário
            buflen;
            n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
            if (n < 0)
                printf("Erro ao ler do socket");
            buflen = ntohl(buflen);
            n = SSL_read(sock, user, buflen);
            if (n < 0)
                printf("Erro ao ler do socket");
            user[n] = '\0';
            printf("Tamanho: %d Mensagem: %s\n", buflen, user);

            logout_user(user);
            printf("Usuário %s deslogado\n", user);
        }
        else if (strcmp(command, "sync_server") == 0)
        {
            printf("Iniciando sincronização de arquivos somente existentes no servidor...\n");
            sync_server_files(user_folder, sock);
            printf("Sincronização finalizada...\n");
        }
        else if (strcmp(command, "list") == 0)
        {
            printf("\n\n Comando List recebido \n\n");
            list(user_folder, sock);
        }
        else if (strcmp(command, "delete_file") == 0)
        {
            printf("\n\n Comando delete_file recebido \n\n");

            char file_name[50];
            //Aguardo nome do arquivo
            buflen;
            n = SSL_read(sock, (char *)&buflen, sizeof(buflen));
            if (n < 0)
                printf("Erro ao ler do socket");
            buflen = ntohl(buflen);
            n = SSL_read(sock, file_name, buflen);
            if (n < 0)
                printf("Erro ao ler do socket");
            file_name[n] = '\0';
            printf("Tamanho: %d Mensagem: %s\n", buflen, file_name);

            delete_file(file_name, user_folder, sock);
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
        printf("Erro ao receber\n");
    }

    free(socket_desc);

    return 0;
}

void list(char *user_folder, SSL *sock)
{
    node_t *cursor = head;
    char str[1000];
    strcpy(str, " ");
    while (cursor != NULL)
    {

        if (strcmp(cursor->cli->userid, user_folder) == 0)
        {
            printf("Usuario encontrado, gerando lista de arquivos...\n");

            int i = 0;
            while (cursor->cli->f_info[i].name[0] != '\0' && cursor->cli->f_info[i].is_deleted == 0)
            {
                strcat(str, "Arquivo: ");
                strcat(str, cursor->cli->f_info[i].name);
                strcat(str, "  Extensao: .");
                strcat(str, cursor->cli->f_info[i].extension);
                strcat(str, "\n");
                i++;
            }
        }
        cursor = cursor->next;
    }

    //Envia a str para o cliente
    int datalen = strlen(str);
    int tmp = htonl(datalen);
    int n = SSL_write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket");
    n = SSL_write(sock, str, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket");

    printf("Lista foi gerada e enviada ao cliente.\n");
}

node_t *create(client_t *cli, node_t *next)
{
    int i = 0;
    node_t *new_node_t = (node_t *)malloc(sizeof(node_t));
    if (new_node_t == NULL)
    {
        printf("Erro criado um novo nodo.\n");
        exit(0);
    }
    new_node_t->cli = cli;
    new_node_t->next = next;

    return new_node_t;
}

node_t *prepend(node_t *head, client_t *cli)
{
    node_t *new_node_t = create(cli, head);
    head = new_node_t;

    return head;
}

int count(node_t *head)
{
    node_t *cursor = head;
    int c = 0;
    while (cursor != NULL)
    {
        c++;
        cursor = cursor->next;
    }

    return c;
}

node_t *append(node_t *head, client_t *cli)
{
    //vai ate o ultimo nodo
    if (head == NULL)
    {
        head = prepend(head, cli);
        return head;
    }
    node_t *cursor = head;

    while (cursor->next != NULL)
    {
        cursor = cursor->next;
    }

    //cria um novo nodo
    node_t *new_node_t = create(cli, NULL);
    cursor->next = new_node_t;

    return head;
}

node_t *create_client_list(node_t *head)
{
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            if (dir->d_type == DT_DIR)
            { // se eh um diretorio
                if (dir->d_name[0] != '.')
                { // se eh um diretorio valido
                    char name[9];
                    strncpy(name, dir->d_name, 8);
                    name[8] = '\0';

                    if (strcmp(name, "sync_dir"))
                    { // se nao eh uma pasta pertencente ao usuario
                        client_t *cli = malloc(sizeof(client_t));
                        cli->devices[0] = 0;
                        cli->devices[1] = 0;
                        strcpy(cli->userid, dir->d_name);

                        int x;

                        //Inicializa nomes com vazio
                        for (x = 0; x < MAXFILES; x++)
                        {
                            strcpy(cli->f_info[x].name, "");
                        }

                        DIR *user_d;
                        struct dirent *user_f;

                        user_d = opendir(cli->userid);
                        if (user_d)
                        {
                            int f_i = 0;

                            while ((user_f = readdir(user_d)) != NULL)
                            {
                                if (user_f->d_name[0] != '.')
                                {
                                    // separa nome e extensao de arquivo
                                    int file_name_i = 0;
                                    // preenche nome do arquivo
                                    while (user_f->d_name[file_name_i] != '.')
                                    {
                                        cli->f_info[f_i].name[file_name_i] = user_f->d_name[file_name_i];
                                        file_name_i++;
                                    }
                                    cli->f_info[f_i].name[file_name_i] = '\0';
                                    file_name_i++;

                                    //preenche extensao do arquivo
                                    int file_extension_i = 0;
                                    while (user_f->d_name[file_name_i] != '\0')
                                    {
                                        cli->f_info[f_i].extension[file_extension_i] = user_f->d_name[file_name_i];
                                        file_extension_i++;
                                        file_name_i++;
                                    }
                                    cli->f_info[f_i].extension[file_extension_i] = '\0';

                                    struct stat attr;
                                    // gera a string do path do arquivo
                                    char path[MAXNAME * 2];
                                    strcpy(path, cli->userid);
                                    path[strlen(cli->userid)] = '/';
                                    strcpy(path + strlen(cli->userid) + 1, user_f->d_name);

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
                        head = prepend(head, cli);

                        printf("Criado cliente. nome: %s. Nome do primeiro arquivo(soh para mostrar q funciona): %s Extensao: %s\n", cli->userid, cli->f_info[0].name, cli->f_info[0].extension);
                    }
                }
            }
        }
        closedir(d);
    }
    return head;
}
