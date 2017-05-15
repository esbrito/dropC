Pra compilar:

gcc -o  client dropboxClient.c

gcc -pthread  -o server dropboxServer.c

Executa o servidor sem parametros, só com ./server

Cliente executa conforme especificado no trabalho, passando user, ip e porta. Ex: ./client eduardo 127.0.0.1 8888

Isso já vai criar as pastas conforme o nome do usuario, tanto no servidor quanto no cliente, caso não existirem ainda.


Dai por enquanto só ta funcionando o upload. Se usa assim: "upload nome_do_arquivo" e ele envia o arquivo pro servidor, que coloca na pasta com o nome do usuário.


Detalhe que o nome do arquivo tem que incluir o path, caso quiser enviar algum arquivo fora da pasta do trabalho. Pra testar eu tava enviando o arquivo teste.txt, usando "upload teste.txt".

