Pra compilar e executar 3 servidores e um cliente:

gcc -pthread  -o client  dropboxClient.c  -lssl -lcrypto


gcc -pthread  -o server dropboxServer.c  -lssl -lcrypto


./server 8585 127.0.0.1 8501 127.0.0.1 9001


./server 9000 127.0.0.1 8586 127.0.0.1 8501


./server 8500 127.0.0.1 8586 127.0.0.1 9001


./client eduardo 127.0.0.1 8585 127.0.0.1 9000 127.0.0.1 8500

