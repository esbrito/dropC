#include "dropboxUtil.h"
#include "dropboxClient.h"

int sock;
struct client user;

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
        perror("Erro ao conectar com o servidor");
    }
    user.logged_in = 1;
    //Envia que usuario que logou
    char username[50];
    strcpy(username, userId);
    strcpy(user.userid, userId);
    if (send(sock, username, sizeof(username), 0) < 0)
    {
        puts("Falha no envio da informação do usuario");
        return 1;
    }
    puts("Conectado com servidor! Seus arquivos serão sincronizados agora...");
    sync_client();
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
        char *word = strsep(&command, " ");
        printf("\nComando digitado: %s\n", word);

        if (strcmp(word, "download") == 0)
        {
            word = strsep(&command, " ");
            //Obtem arquivo especificado
            get_file(word);
        }

        else if (strcmp(word, "upload") == 0)
        {
            char upload_command[10];
            char upload_response[10];
            strcpy(upload_command, "upload");
            send(sock, upload_command, strlen(upload_command), 0); //Envia apenas o comando

            //Aguarda confirmação de recebimento
            n = recv(sock, upload_response, sizeof(upload_response), 0);
            upload_response[n] = '\0';
            if (strcmp(upload_response, "upload") == 0)
            {
                word = strsep(&command, " \n");
                printf("Arquivo a ser enviado: ->%s<- \n", word);
                //Envia arquivo especificado
                send_file(word);
            }
            else
            {
                printf("Falha ao receber confirmação de comando enviado\n");
            }
        }

        else if (strcmp(word, "list") == 0) {
            char list_command[10];
            char list_response[10];
            strcpy(list_command,"list");
            send(sock, list_command, strlen(list_command),0); // Envia apenas o comando

            //Aguarda confirmacao de recebimento
            n = recv(sock, list_response, sizeof(list_response),0);
            list_response[n] = '\n';
            if (strcmp(list_response, "list") == 0) {
                //Aguarda listagem de arquivos
                
            }
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
        perror("Erro ao criar socket");
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

void sync_client()
{
    printf("\nSync...\n");
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "sync_dir_%s", user.userid);
    printf("Verificando cliente %s\n", user.userid);
    printf("Verificando existência da pasta %s\n", buffer);
    struct stat st = {0};
    if (stat(buffer, &st) == -1)
    {
        printf("Não existe! Criando pasta...\n");
        if (mkdir(buffer, 0777) < 0)
        {
            perror("Erro ao criar pasta...\n");
        }
    }
    //TODO realizar verificação dos arquivos e sinconizar de verdade
}

void get_file(char *file)
{
}

void send_file(char *file)
{
    //Enviando nome do arquivo
    int n = 0;
    char file_name[50];
    char file_name_response[50];
    snprintf(file_name, sizeof(file_name), "%s/%s", user.userid, file);
    send(sock, file_name, strlen(file_name), 0); //Envia o nome do arquivo já com o path da pasta do servidor para facilitar. Ex. eduardo/arquivo.txt
    //Aguarda confirmação de recebimento do nome
    n = recv(sock, file_name_response, sizeof(file_name_response), 0);
    file_name_response[n] = '\0';
    if (strcmp(file_name_response, file_name) == 0)
    {
        printf("Confirmado o nome do arquivo a ser enviado\n");
        FILE *fp;

        printf("Verificando se arquivo existe: ->%s<- \n", file);
        if ((fp = fopen(file, "r")) == NULL)
        {
            printf("Arquivo não encontrado\n");
        }
        else
        {
            char file_buffer[1000];
            char f_buffer[1000];
            printf("\nArquivo encontrado\n");
            while (!feof(fp)) //até acabar o arquivo
            {
                printf("Lendo arquivo...\n");
                fgets(f_buffer, 1000, fp); //extrai 1000 chars do arquivo
                if (feof(fp))
                    break;
                strcat(file_buffer, f_buffer);
            }
            printf("Enviando arquivo para o servidor....\n");
            fclose(fp);
            send(sock, file_buffer, strlen(file_buffer), 0);
            //Aguarda confirmação de recebimento do arquivo
            char upload_done[10];
            n = recv(sock, upload_done, sizeof(upload_done), 0);
            upload_done[n] = '\0';
            if (strcmp(upload_done, "done") == 0)
            {
                printf("Arquivo recebido com sucesso!\n");
            }
            else
            {
                printf("Falha na confirmação do recebimento do arquivo.\n");
            }
        }
    }
    else
    {
        printf("Erro ao enviar o nome do arquivo");
    }
}