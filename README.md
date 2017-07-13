Pra compilar:

gcc -pthread  -o  client dropboxClient.c

gcc -pthread  -o server dropboxServer.c

Executa o servidor sem parametros, só com ./server

Cliente executa conforme especificado no trabalho, passando user, ip e porta. Ex: ./client eduardo 127.0.0.1 8888
