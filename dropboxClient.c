#include "dropboxUtil.h"
#include "dropboxClient.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <limits.h>

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

int sock;
struct client user;
char client_folder[1024];
char username[50];
int inotifyFd, wd;
char buf[BUF_LEN] __attribute__((aligned(8)));
ssize_t numRead;
char *p;
struct inotify_event *event;

int main(int argc, char *argv[])
{
    int port;
    char *userId = argv[1];
    char *ip = argv[2];
    char *portArg = argv[3];
    struct sockaddr_in server;
    char message[1000], server_reply[2000];
    int n;
    char buf_recv[1000];
    char buf_send[1000];
    char *filename;

    port = atoi(portArg);

    if (connect_server(ip, port) == 1)
    {
        printf("Erro ao conectar com o servidor");
    }

    //Envia qual usuario que logou
    strcpy(username, userId);
    int datalen = strlen(username);
    int tmp = htonl(datalen);
    n = write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket");
    n = write(sock, username, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket");

    //Aguarda resposta do servidor se de fato logou
    char ok_to_connect[10];
    int buflen;
    n = read(sock, (char *)&buflen, sizeof(buflen));
    if (n < 0)
        printf("Erro ao ler do socket");
    buflen = ntohl(buflen);
    n = read(sock, ok_to_connect, buflen);
    if (n < 0)
        printf("Erro ao ler do socket");
    ok_to_connect[n] = '\0';

    //Descontecta caso já tiver dois usuários logados
    if (strcmp(ok_to_connect, "un_auth") == 0)
    {
        printf("Já existe mais de 2 usuários conectados!!!");
        close_connection();
    }
    puts("Conectado com servidor!");
    printf("\nOperações disponíveis: \n\n");
    printf("\t>> upload <path/filename.ext> \n\n");
    printf("\t>> download <filename.ext> \n\n");
    printf("\t>> list \n \n");
    printf("\t>> get_sync_dir \n \n");
    printf("\t>> exit \n \n \n");
    char command[50];
    char file_command_name[50];
    while (1)
    {

        printf(">>");
        fgets(message, 100, stdin);
        char *command = strdup(message);

        if (command[4] == 10)
        { // Corrige espaço do strdup para que não seja necessário enviar "list " por exemplo
            command[4] = '\0';
        }
        if (command[12] == 10)
        { // Corrige espaço do strdup para que não seja necessário enviar "get_sync_dir " por exemplo
            command[12] = '\0';
        }

        char *word = strsep(&command, " ");

        //Verifica qual dos comandos foi solicitado pelo cliente
        printf("\nComando digitado: %s\n", word);

        if (strcmp(word, "download") == 0)
        {
            char download_command[20];
            strcpy(download_command, "download");

            //Envia o comando
            int datalen = strlen(download_command);
            int tmp = htonl(datalen);
            int n = write(sock, (char *)&tmp, sizeof(tmp));
            if (n < 0)
                printf("Erro ao escrever no socket");
            n = write(sock, download_command, datalen);
            if (n < 0)
                printf("Erro ao escrever no socket");

            word = strsep(&command, " \n");
            printf("Arquivo a ser recebido: ->%s<- \n", word);

            //Envia o nome do arquivo
            datalen = strlen(word);
            tmp = htonl(datalen);
            n = write(sock, (char *)&tmp, sizeof(tmp));
            if (n < 0)
                printf("Erro ao escrever no socket");
            n = write(sock, word, datalen);
            if (n < 0)
                printf("Erro ao escrever no socket");

            char file_name_with_extension[50];

            //Aguardo confirmação
            int buflen;
            n = read(sock, (char *)&buflen, sizeof(buflen));
            if (n < 0)
                printf("Erro ao ler do socket");
            buflen = ntohl(buflen);
            n = read(sock, file_name_with_extension, buflen);
            if (n < 0)
                printf("Erro ao ler do socket");
            file_name_with_extension[n] = '\0';

            get_file(word);
        }
        else if (strcmp(word, "get_sync_dir") == 0)
        {
            printf("A partir de agora seus arquivos serão sincronizados...");
            sync_client();
        }
        else if (strcmp(word, "exit") == 0)
        {
            printf("Saindo...");
            char exit_command[10];
            strcpy(exit_command, "exit");

            //Envia o comando para avisar que está deslogando
            int datalen = strlen(exit_command);
            tmp = htonl(datalen);
            n = write(sock, (char *)&tmp, sizeof(tmp));
            if (n < 0)
                printf("Erro ao escrever no socket");
            n = write(sock, exit_command, datalen);
            if (n < 0)
                printf("Erro ao escrever no socket");

            //Envia nome do usuário que vai deslogar
            datalen = strlen(username);
            tmp = htonl(datalen);
            n = write(sock, (char *)&tmp, sizeof(tmp));
            if (n < 0)
                printf("Erro ao escrever no socket");
            n = write(sock, username, datalen);
            if (n < 0)
                printf("Erro ao escrever no socket");

            close_connection();
        }
        else if (strcmp(word, "upload") == 0)
        {

            word = strsep(&command, " \n");

            char file[50];
            char file_name[50];
            char file_extension[50];

            printf("Arquivo a ser enviado: ->%s<- \n", word);
            strcpy(file, word);

            //Separa extensão do nome do arquivo
            int file_name_i = 0;
            // preenche nome do arquivo
            while (file[file_name_i] != '.')
            {
                file_name[file_name_i] = file[file_name_i];
                file_name_i++;
            }
            file_name[file_name_i] = '\0';

            printf("Nome do arquivo: %s\n", file_name);
            file_name_i++;
            //preenche extensao do arquivo
            int file_extension_i = 0;
            while (file[file_name_i] != '\0')
            {
                file_extension[file_extension_i] = file[file_name_i];
                file_extension_i++;
                file_name_i++;
            }
            file_extension[file_extension_i] = '\0';
            printf("Extensão do arquivo: %s\n", file_extension);

            //Envia arquivo especificado
            FILE *fp;
            printf("Verificando se arquivo existe: ->%s<- \n", word);

            if ((fp = fopen(word, "r")) == NULL)
            {
                printf("Arquivo não encontrado\n");
            }
            else
            {
                char upload_command[20];
                strcpy(upload_command, "upload");

                //Envia o comando para iniciar o upload
                int datalen = strlen(upload_command);
                int tmp = htonl(datalen);
                int n = write(sock, (char *)&tmp, sizeof(tmp));
                if (n < 0)
                    printf("Erro ao escrever no socket");
                n = write(sock, upload_command, datalen);
                if (n < 0)
                    printf("Erro ao escrever no socket");

                printf("\nArquivo encontrado\n");

                //Envia last modified do arquivo
                struct stat attr;
                stat(word, &attr);
                time_t last_modified = attr.st_mtime;
                int lm = htonl(last_modified);

                datalen = sizeof(lm);
                tmp = htonl(datalen);
                n = write(sock, (char *)&tmp, sizeof(tmp));
                if (n < 0)
                    printf("Erro ao escrever no socket");
                n = write(sock, &lm, datalen);
                if (n < 0)
                    printf("Erro ao escrever no socket");

                send_file(file_name, file_extension, fp);
            }
        }

        else if (strcmp(word, "list") == 0)
        {
            char list_command[20];
            strcpy(list_command, "list");

            //Envia comando
            int datalen = strlen(list_command);
            int tmp = htonl(datalen);
            int n = write(sock, (char *)&tmp, sizeof(tmp));
            if (n < 0)
                printf("Erro ao escrever no socket");
            n = write(sock, list_command, datalen);
            if (n < 0)
                printf("Erro ao escrever no socket");

            char str[1000];

            //Aguarda a string contendo a lista
            int buflen;
            n = read(sock, (char *)&buflen, sizeof(buflen));
            if (n < 0)
                printf("Erro ao ler do socket");
            buflen = ntohl(buflen);
            n = read(sock, str, buflen);
            if (n < 0)
                printf("Erro ao ler do socket");
            str[n] = '\0';
            puts(str);
        }

        else
        {
            printf("Comando invalido...\n");
        }
    }

    close(sock);
    return 0;
}

int connect_server(char *host, int port)
{
    struct sockaddr_in server;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        printf("Erro ao criar socket");
        return 1;
    }

    server.sin_addr.s_addr = inet_addr(host);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    //Conecta ao servidor
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        return 1;
    }
}

void ask_for_server_files()
{
    //Envia comando para obter arquivos somente existentes no servidor
    char sync_server_command[20];
    strcpy(sync_server_command, "sync_server");

    //Envia o comando para iniciar sincronização
    int datalen = strlen(sync_server_command);
    int tmp = htonl(datalen);
    int n = write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket");
    n = write(sock, sync_server_command, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket");

    //Vai ficar recebendo arquivos até servidor avisar que enviou todos que o cliente nao possuia
    char file_name_with_extension[50];
    while (!(strcmp(file_name_with_extension, "server_done") == 0))
    {
        //Aguardo nome do arquivo
        int buflen;
        n = read(sock, (char *)&buflen, sizeof(buflen));
        if (n < 0)
            printf("Erro ao ler do socket");
        buflen = ntohl(buflen);
        n = read(sock, file_name_with_extension, buflen);
        if (n < 0)
            printf("Erro ao ler do socket");
        file_name_with_extension[n] = '\0';

        if ((strcmp(file_name_with_extension, "server_done") == 0))
            break;

        FILE *fp;
        //Verificando se arquivo existe
        if ((fp = fopen(file_name_with_extension, "r")) == NULL)
        {
            //Arquivo somente no servidor. Requisitando download
            char response[20];
            strcpy(response, "new_file");

            //Envia comando avisando para baixar arquivo pois nao existe no cliente só no servidor
            datalen = strlen(response);
            tmp = htonl(datalen);
            n = write(sock, (char *)&tmp, sizeof(tmp));
            if (n < 0)
                printf("Erro ao escrever no socket");
            n = write(sock, response, datalen);
            if (n < 0)
                printf("Erro ao escrever no socket");

            //Aguardo nome do arquivo
            int buflen;
            n = read(sock, (char *)&buflen, sizeof(buflen));
            if (n < 0)
                printf("Erro ao ler do socket");
            buflen = ntohl(buflen);
            n = read(sock, file_name_with_extension, buflen);
            if (n < 0)
                printf("Erro ao ler do socket");
            file_name_with_extension[n] = '\0';

            get_file(file_name_with_extension);
        }
        else
        {

            char response[20];
            strcpy(response, "has_file_already");

            //Envia comando avisando para nao baixar arquivo pois existe no cliente
            datalen = strlen(response);
            tmp = htonl(datalen);
            n = write(sock, (char *)&tmp, sizeof(tmp));
            if (n < 0)
                printf("Erro ao escrever no socket");
            n = write(sock, response, datalen);
            if (n < 0)
                printf("Erro ao escrever no socket");
        }
    }
}

void *read_local_files()
{

    while (1)
    {
        printf("\nVarrendo arquivos locais... FAVOR NAO REALIZAR NENHUM COMANDO\n");

        //Faz a verificaçãao se algum arquivo foi movido ou deletado'
        numRead = read(inotifyFd, buf, BUF_LEN);

        for (p = buf; p < buf + numRead;)
        {

            event = (struct inotify_event *)p;
            //Verifica tipo de evento

            if (event->mask & IN_MOVED_FROM)
            {

                //Foi deletado para a lixeira ou movido. Avisar o servidor

                //Envia o comando para deletar
                char delete_command[20];
                strcpy(delete_command, "delete_file");
                int datalen = strlen(delete_command);
                int tmp = htonl(datalen);
                int n = write(sock, (char *)&tmp, sizeof(tmp));
                if (n < 0)
                    printf("Erro ao escrever no socket");
                n = write(sock, delete_command, datalen);
                if (n < 0)
                    printf("Erro ao escrever no socket");

                //Envia nome do arquivo para deletar
                datalen = strlen(event->name);
                tmp = htonl(datalen);
                n = write(sock, (char *)&tmp, sizeof(tmp));
                if (n < 0)
                    printf("Erro ao escrever no socket");
                n = write(sock, event->name, datalen);
                if (n < 0)
                    printf("Erro ao escrever no socket");

                //Recebe confirmacao do servidor
                char response[20];
                int buflen;
                n = read(sock, (char *)&buflen, sizeof(buflen));
                if (n < 0)
                    printf("Erro ao ler do socket");
                buflen = ntohl(buflen);
                n = read(sock, response, buflen);
                if (n < 0)
                    printf("Erro ao ler do socket");
                response[n] = '\0';
                if (strcmp(response, "done"))
                {
                    printf("Arquivo deletado com sucesso no servidor!\n");
                }
            }

            p += sizeof(struct inotify_event) + event->len;
        }

        // Agora varre arquivos locais para sincronizá-los
        DIR *user_d;
        struct dirent *user_f;
        int read_size;
        // Abre pasta do cliente
        user_d = opendir(client_folder);
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

                    //Envia comando para sincronizar este arquivo
                    char sync_local_command[20];
                    strcpy(sync_local_command, "sync_local");

                    //Envia o comando para iniciar sincronização
                    int datalen = strlen(sync_local_command);
                    int tmp = htonl(datalen);
                    int n = write(sock, (char *)&tmp, sizeof(tmp));
                    if (n < 0)
                        printf("Erro ao escrever no socket");
                    n = write(sock, sync_local_command, datalen);
                    if (n < 0)
                        printf("Erro ao escrever no socket");

                    struct stat attr;
                    // gera a string do path do arquivo
                    char path[MAXNAME * 2];
                    snprintf(path, sizeof(path), "%s/%s.%s", client_folder, current_local_file.name, current_local_file.extension);

                    //Envia nome do arquivo
                    datalen = strlen(current_local_file.name);
                    tmp = htonl(datalen);
                    n = write(sock, (char *)&tmp, sizeof(tmp));
                    if (n < 0)
                        printf("Erro ao escrever no socket");
                    n = write(sock, current_local_file.name, datalen);
                    if (n < 0)
                        printf("Erro ao escrever no socket");

                    stat(path, &attr);
                    current_local_file.last_modified = attr.st_mtime;

                    // envia o last modified para o servidor
                    int lm = htonl(current_local_file.last_modified);
                    datalen = sizeof(lm);
                    tmp = htonl(datalen);
                    n = write(sock, (char *)&tmp, sizeof(tmp));
                    if (n < 0)
                        printf("Erro ao escrever no socket");
                    n = write(sock, &lm, datalen);
                    if (n < 0)
                        printf("Erro ao escrever no socket");

                    char response[50];

                    //Aguardo resposta do servidor do que fazer
                    int buflen;
                    n = read(sock, (char *)&buflen, sizeof(buflen));
                    if (n < 0)
                        printf("Erro ao ler do socket");
                    buflen = ntohl(buflen);
                    n = read(sock, response, buflen);
                    if (n < 0)
                        printf("Erro ao ler do socket");
                    response[n] = '\0';

                    if (strcmp(response, "new_file") == 0)
                    {
                        //Arquivo não encontrado no servidor, será enviado agora
                        //Envia arquivo especificado
                        char file_name_with_extension[50];
                        snprintf(file_name_with_extension, sizeof(file_name_with_extension), "sync_dir_%s/%s.%s", username, current_local_file.name, current_local_file.extension);
                        FILE *fp;
                        printf("Verificando se arquivo existe: ->%s<- \n", file_name_with_extension);
                        if ((fp = fopen(file_name_with_extension, "r")) == NULL)
                        {
                            printf("Arquivo não encontrado\n");
                        }
                        else
                        {
                            //Arquivo encontrado

                            send_file(current_local_file.name, current_local_file.extension, fp);
                        }
                    }
                    if (strcmp(response, "updatelocal") == 0)
                    {
                        //Arquivo do servidor mais novo. Necessita atualizar a pasta do usuario
                        char file_name_with_extension[50];
                        //Aguardo nome do arquivo
                        int buflen;
                        n = read(sock, (char *)&buflen, sizeof(buflen));
                        if (n < 0)
                            printf("Erro ao ler do socket");
                        buflen = ntohl(buflen);
                        n = read(sock, file_name_with_extension, buflen);
                        if (n < 0)
                            printf("Erro ao ler do socket");
                        file_name_with_extension[n] = '\0';
                        get_file(file_name_with_extension);
                    }
                    else if (strcmp(response, "updateserver") == 0)
                    {
                        //Arquivo do usuario mais novo. Necessita atualizar o servidor
                        char file_name_with_extension[50];
                        snprintf(file_name_with_extension, sizeof(file_name_with_extension), "sync_dir_%s/%s.%s",
                                 username, current_local_file.name, current_local_file.extension);

                        FILE *fp;

                        //Verificando se arquivo existe
                        if ((fp = fopen(file_name_with_extension, "r")) == NULL)
                        {
                            printf("Arquivo não encontrado\n");
                        }
                        else
                        {
                            //Arquivo encontrado
                            send_file(current_local_file.name, current_local_file.extension, fp);
                        }
                    }
                    else if (strcmp(response, "iguais") == 0)
                    {
                        //Arquivo do usuario e do servidor sao iguais
                    }
                    else if (strcmp(response, "delete_file") == 0)
                    {
                        //Deleta arquivo do usuário
                        char path_with_file[50];
                        snprintf(path_with_file, sizeof(path_with_file), "%s/%s.%s",
                                 client_folder, current_local_file.name, current_local_file.extension);

                        int ret = remove(path_with_file);
                        if (ret == 0)
                        {
                            printf("Arquivo deletado com sucesso!");
                        }
                        else
                        {
                            printf("Erro ao deletar arquivo");
                        }
                    }
                }
            }
        }
        closedir(user_d);
        ask_for_server_files();
        printf("\nSincronização concluida \n\n");

        //Limpa a tela
        const char *CLEAR_SCREEN_ANSI = "\e[1;1H\e[2J";
        write(STDOUT_FILENO, CLEAR_SCREEN_ANSI, 12);

        //Apresenta menu novamente
        printf("\nOperações disponíveis: \n\n");
        printf("\t>> upload <path/filename.ext> \n\n");
        printf("\t>> download <filename.ext> \n\n");
        printf("\t>> list \n \n");
        printf("\t>> get_sync_dir \n \n");
        printf("\t>> exit \n \n \n");
        printf(">>");
        fflush(stdout);
        sleep(20);
    }
}

void close_connection()
{
    close(sock);
    exit(0);
}

void sync_client()
{

    printf("\nSync...\n");
    snprintf(client_folder, sizeof(client_folder), "sync_dir_%s", username);
    printf("Verificando cliente %s\n", username);
    printf("Verificando existência da pasta %s\n", client_folder);
    struct stat st = {0};
    if (stat(client_folder, &st) == -1)
    {
        printf("Não existe! Criando pasta...\n");
        if (mkdir(client_folder, 0777) < 0)
        {
            printf("Erro ao criar pasta...\n");
        }
    }

    printf("\nInicializando sincronização...\n");
    pthread_t sync_thread;
    pthread_t notify_thread;
    inotifyFd = inotify_init1(IN_NONBLOCK); /* Cria instância não bloqueante do inotify (para ele não bloquear na hora de ler o buffer caso nenhum evento ocorreu) */
    if (inotifyFd == -1)
        printf("inotify_init");

    wd = inotify_add_watch(inotifyFd, client_folder, IN_ALL_EVENTS);
    if (wd == -1)
        printf("inotify_add_watch");
    printf("Watching %s using wd %d\n", client_folder, wd);

    //Cria thread de sincronização em background
    if (pthread_create(&sync_thread, NULL, read_local_files, NULL) < 0)
    {
        printf("Falha ao criar a thread");
    }
}

void get_file(char *file)
{

    char file_buffer[100000];
    FILE *fp;
    int n;

    //Recebe arquivo
    int buflen;
    n = read(sock, (char *)&buflen, sizeof(buflen));
    if (n < 0)
        printf("Erro ao ler do socket");
    buflen = ntohl(buflen);
    n = read(sock, file_buffer, buflen);
    if (n < 0)
        printf("Erro ao ler do socket");
    file_buffer[n] = '\0';

    fflush(stdout);
    unlink(file);
    fp = fopen(file, "w");
    fputs(file_buffer, fp);
    fclose(fp);

    //Limpa buffer
    memset(file_buffer, 0, sizeof(file_buffer));

    printf("\n\n Arquivo recebido \n\n");
    char response[50];
    strcpy(response, "done");

    //Envia confirmacao de recebimento
    int datalen = strlen(response);
    int tmp = htonl(datalen);
    n = write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket");
    n = write(sock, response, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket");
}

void send_file(char *file_name, char *file_extension, FILE *fp)
{

    //Envia o nome do arquivo
    int datalen = strlen(file_name);
    int tmp = htonl(datalen);
    int n = write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket");
    n = write(sock, file_name, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket");

    //Envia a extensão do arquivo
    datalen = strlen(file_extension);
    tmp = htonl(datalen);
    n = write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket");
    n = write(sock, file_extension, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket");

    printf("Enviando arquivo...\n");

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
    n = write(sock, (char *)&tmp, sizeof(tmp));
    if (n < 0)
        printf("Erro ao escrever no socket");
    n = write(sock, file_buffer, datalen);
    if (n < 0)
        printf("Erro ao escrever no socket");

    free(file_buffer);

    //Aguarda resposta de recebimento completo do arquivo no servidor
    char response[20];
    int buflen;
    n = read(sock, (char *)&buflen, sizeof(buflen));
    if (n < 0)
        printf("Erro ao ler do socket");
    buflen = ntohl(buflen);
    n = read(sock, response, buflen);
    if (n < 0)
        printf("Erro ao ler do socket");
    response[n] = '\0';
    if (strcmp(response, "done"))
    {
        printf("Arquivo recebido com sucesso!\n");
    }
}
