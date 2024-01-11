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
#define MAX_RETRIES 5  //número de intentos de retransmision

//Estructura del paquete
typedef struct {
	int seq_number;
	int sender_id;
	char data[BUF_SIZE];
} Packet;

//Estructura del ACK
typedef struct {
	int ack_number;
} AckPacket;

int main(int argc, char *argv[]) {

	if (argc != 4) {
		fprintf(stderr, "Uso: %s <host_destino> <puerto_destino> <nombre_archivo> \n", argv[0]);
		exit(EXIT_FAILURE);
	}
	

	const char *host_destino = argv[1];
	int puerto_destino = atoi(argv[2]);
	const char *nombre_archivo = argv[3];

	FILE *file = fopen(nombre_archivo, "rb");
	if (file == NULL) {
		perror("Error al abrir el archivo");
		exit(EXIT_FAILURE);
	}

	int sockfd;
	struct sockaddr_in dest_addr;
	socklen_t addrlen = sizeof(dest_addr);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0){
		perror("Error al crear el socket");
		exit(EXIT_FAILURE);
	}

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(puerto_destino);
	inet_aton(host_destino, &dest_addr.sin_addr);
	
	
	Packet packet;
	int seq_number = 0;
	int sender_id = getpid(); //Así se obtendrá de manera única a cada emisor
	int bytes_read;
	int retries = 0;
	
	AckPacket ack_packet;

	sem_t *socket_semaphore;
	socket_semaphore = sem_open("socket_semaphore", O_CREAT, 0644, 1);
	if (socket_semaphore == SEM_FAILED){
		perror("Error al crear el semáforo");
		exit(EXIT_FAILURE);
	}


	while ((bytes_read = fread(packet.data, 1, BUF_SIZE, file)) > 0){
		sem_wait(socket_semaphore); //Bloquea el acceso al socket

		packet.seq_number = seq_number++;
		packet.sender_id = sender_id;

		//Envío del paquete
		sendto(sockfd, &packet, PACKET_SIZE, 0, (struct sockaddr *)&dest_addr, addrlen);

		//Espera confirmación del receptor
		int recv_success = 0;
		while (!recv_success){

			//Espera confirmación del receptor (ACK)
			recvfrom(sockfd, &ack_packet, sizeof(AckPacket), 0, (struct sockaddr *)&dest_addr, &addrlen);

			//Si se recibe la confirmación, salir del bucle de retransmisión
			//Si no, retransmitir el paquete
			if(ack_packet.ack_number == packet.seq_number) {
				recv_success = 1;
			} else {
				//Retransmitir el mensaje
				sendto(sockfd, &packet, PACKET_SIZE, 0, (struct sockaddr *)&dest_addr, addrlen);

				retries++;

				if(retries == MAX_RETRIES) {
					fprintf(stderr, "Máximo de intentos alcanzado. Interrumpiendo la transmisión");
					fclose(file);
					close(sockfd);
					exit(EXIT_FAILURE);
				}
			}
		}
		memset(&packet, 0, PACKET_SIZE);
		sem_post(socket_semaphore); //Libera el acceso al socket
	}
	retries = 0;
	fclose(file);
	close(sockfd);
	sem_close(socket_semaphore);
	sem_unlink("socket_semaphore");

	return 0;
}
