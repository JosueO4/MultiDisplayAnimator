#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 12345
#define BUFFER_SIZE 2048

int main() {

    int sock = 0;                       
    struct sockaddr_in serv_addr;      
    char buffer[BUFFER_SIZE] = {0};     

    // Creación del socket TCP
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket falló");
        exit(1);
    }

    // Configuración de la dirección del servidor

    serv_addr.sin_family = AF_INET;           
    serv_addr.sin_port = htons(PORT);         


    // Conversión de dirección IP
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Dirección inválida");
        exit(1);
    }


    // Conexión al servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Conexión fallida");
        exit(1);
    }

    while (1) {

        // Recibir datos del servidor

        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);

        if (bytes <= 0) break;  
        
        buffer[bytes] = '\0';  

        // Limpiar pantalla y mostrar nueva información

        //printf("\033[H\033[2J");
        printf("%s", buffer);    
        fflush(stdout);          
    }

    close(sock);  
    return 0;   
}
