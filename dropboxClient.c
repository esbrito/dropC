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

    strcpy(user.userid, userId);
    if (connect_server(ip, port) == 1)
    {
        perror("Erro ao conectar com o servidor");
    }
    user.logged_in = 1;
    //Envia que usuario que logou
    if (send(sock, &user, sizeof(user), 0) < 0)
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
            printf("Realizando upload...");
            char upload_command[10];
            char upload_response[10];
            strcpy(upload_command, "upload");
            send(sock, upload_command, strlen(upload_command), 0); //Envia apenas o comando
            //Aguarda confirmação de recebimento
            n = recv(sock, upload_response, sizeof(upload_response), 0);
            upload_response[n] = '\0';
            if (strcmp(word, "upload") == 0){
            word = strsep(&command, " ");
            //Envia arquivo especificado
            send_file(word);
            }
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
    FILE *fp;
    printf("Verificando se arquivo existe: %s \n", file);
    if ((fp = fopen("teste.txt", "r")) == NULL)
    {
        //tratar erro
    }
    else
    {
        char file_buffer[1000];
        char f_buffer[1000];
        printf("\nArquivo encontrado. Fazendo o envio\n");
        printf("Enviando arquivo para o servidor....\n");
        while (!feof(fp)) //até acabar o arquivo
        {
            printf("Lendo arquivo...\n");
            fgets(f_buffer, 1000, fp); //extrai 1000 chars do arquivo
            if (feof(fp))
                break;
            strcat(file_buffer, f_buffer);
        }
        fclose(fp);
        send(sock, file_buffer, strlen(file_buffer), 0); //sends 1000 extracted byte
    }
}