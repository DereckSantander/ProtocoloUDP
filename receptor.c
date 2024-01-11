#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <semaphore.h>

#define BUF_SIZE 1024
#define PACKET_SIZE 1032

//Estructura del paquete
typedef struct {
	int seq_number;
	int sender_id;
	char data[BUF_SIZE];
} Packet;

typedef struct {
	int ack_number;
} AckPacket;

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        	exit(EXIT_FAILURE);
    	}

    	int puerto = atoi(argv[1]);

    	int sockfd;
    	struct sockaddr_in server_addr, client_addr;
    	socklen_t addrlen = sizeof(client_addr);

    	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    	if (sockfd < 0) {
        	perror("Error al crear el socket");
        	exit(EXIT_FAILURE);
    	}

    	memset(&server_addr, 0, sizeof(server_addr));
    	server_addr.sin_family = AF_INET;
    	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    	server_addr.sin_port = htons(puerto);

    	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        	perror("Error al hacer el bind");
        	exit(EXIT_FAILURE);
    	}

    	Packet packet;
    	FILE *file;

    	int expected_seq_number = 0;

	AckPacket ack_packet;

	sem_t *socket_semaphore;
	socket_semaphore = sem_open("socket_semaphore", O_CREAT, 0644, 1);
	if(socket_semaphore == SEM_FAILED){
		perror("Error al abrir el semáforo");
		exit(EXIT_FAILURE);
	}

    	while (1) {
		sem_wait(socket_semaphore); //Bloquea el acceso al archivo

        	recvfrom(sockfd, &packet, PACKET_SIZE, 0, (struct sockaddr *)&client_addr, &addrlen);

		char filename[20];
		sprintf(filename, "archivo_%d.txt", packet.sender_id);
		file = fopen(filename, "a");
		if(file == NULL) {
			perror("Error al abrir el archivo");
			exit(EXIT_FAILURE);
		}
        	// Verificación de secuencia y manejo de duplicados
        	if (packet.seq_number == expected_seq_number) {
            		fwrite(packet.data, 1, BUF_SIZE, file);
            		expected_seq_number++;

			//Enviar confirmación al emisor (ACK)
			ack_packet.ack_number = packet.seq_number;
			sendto(sockfd, &ack_packet, sizeof(AckPacket), 0, (struct sockaddr *)&client_addr, addrlen);
       		} else {
            		// Se debería enviar una confirmación al emisor para la retransmisión
            		ack_packet.ack_number = expected_seq_number - 1;
			sendto(sockfd, &ack_packet, sizeof(AckPacket), 0, (struct sockaddr *)&client_addr, addrlen);
        	}

		fclose(file);
	
		sem_post(socket_semaphore); //Libera el acceso al socket
    	}
	printf("Paquete recibido con exito\n");
    	close(sockfd);
	sem_close(socket_semaphore);

    	return 0;
}
