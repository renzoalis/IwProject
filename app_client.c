#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
    1. Definir Nombre del Servidor
    2. Definir Puerto del Servidor
*/

#define MYNAME "ServerName"	//Nombre (default)
#define MYPORT 3490		//Puerto (default)
#define BACKLOG 10		//Cantidad máxima de conexiones
#define DATASIZE 1024		//Tamaño máximo del dato

/*--------------------------------------------------------------------------------+
 +                                ESTRUCTURAS                                     +
 +------------------------------------------------------------------------------ */

/*--------------------------------------------------------------------------------+
 + NOMBRE: 'data'                                                                 +
 + OBJETIVO: Almacenar los datos recibidos por parte de 'app_server'              +
 +-------------------------------------------------------------------------------*/

typedef struct {
    char app_name[50];
    char repo[50];
    char user[50];
    char branch[50];
    char path[100];
    int option;
} data;


/*--------------------------------------------------------------------------------+
 +                                 FUNCIONES                                      +
 +-------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------+
 + NOMBRE: 'send_result'                                                          +
 + PARÁMETROS: fd (socket) - command (comando)                                    +
 + OBJETIVO: Enviar el resultado de ejecución del comando a 'app_server'          +
 + DESCRIPCIÓN: El resultado de la ejecución del comando se almacena en un        +
 +              archivo, luego se lee y envía el resultado                        +
 +-------------------------------------------------------------------------------*/

void send_result (int fd, char *command)
{
    FILE *fp;
    char res[DATASIZE];

    fp = popen (command, "r");
    if (fp == NULL) {
	printf ("Error en la ejecución del comando");
	exit(1);
    }
    fgets (res, DATASIZE, fp);
    printf ("Resultado: %s\n", res);
    send (fd, &res, (sizeof(char) * DATASIZE), 0);
    pclose (fp);
}

/*--------------------------------------------------------------------------------+
 +                                MÓDULO PRINCIPAL                                +
 +-------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------+
 + OBJETIVO: Realizar la ejecución de 'app_cliente'                               +
 + DESCRIPCIÓN: Crea e inicializa las funciones de la biblioteca BSD Socket       +
 +              Recibe una única conexión a la vez, por parte de 'app_server' y   +
 +              realiza la tarea solicitada                                       +
 +-------------------------------------------------------------------------------*/
int main ()
{
    /*Variables generales*/
    data recv_data;
    char *command;
    char *cd = "(cd ";
    char result[DATASIZE];

    /*Variables socket*/
    int sockfd, new_fd, n;
    struct sockaddr_in sv_addr;
    struct sockaddr_in cl_addr;
    int sin_size;

    system("clear");

    /*Inicializar socket*/
    if ( (sockfd = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1 ) {
	perror ("socket");
	exit (1);
    }

    /*Liberar ADDRESS luego de su uso*/
    int yes = 1;
    if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ) {
	perror ("setsockopt");
	exit(1);
    }

    /*Asignar la correspondiente información de 'app_client'*/
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_port = htons (MYPORT);
    sv_addr.sin_addr.s_addr = INADDR_ANY;
    memset (&(sv_addr.sin_zero), '\0', 8);

    /*Asignar socket a una dirección*/
    if ( bind(sockfd, (struct sockaddr*)&sv_addr, sizeof(struct sockaddr)) == -1  ) {
	perror ("bind");
	exit (1);
    }

    /*Esperando conexiones*/
    if ( listen(sockfd, BACKLOG) == -1 ) {
	perror ("listen");
	exit (1);
    }

    printf ("\t\tSERVER: %s. PORT: %d\n\n", MYNAME, MYPORT);

    while (1) {
	/*Acepta nueva conexión entrante - Crea un nuevo 'file descriptor'*/
	sin_size = sizeof (struct sockaddr_in);
	if ( (new_fd = accept(sockfd, (struct sockaddr*) &cl_addr, &sin_size)) == -1 ) {
	    perror ("accept");
	    exit (1);
	}

	/*Recibir mensaje - Lo almacena en la estructura 'DATA'*/
        if ( (n = recv ( new_fd, (char *)&recv_data, sizeof(data), 0)) == -1 ) {
	    perror ("recv");
	    exit (1);
	}
	printf ("\nAPP: %s. REPO: %s. BRANCH: %s. PATH: %s\n", recv_data.app_name, recv_data.repo, recv_data.branch, recv_data.path);

	/*Según el valor del campo 'opción' recibido, realiza una determinada tarea*/
	switch(recv_data.option) {
	    /*Consultar*/
	    case 1:
			printf ("Solicitud del último commit de la aplicación: '%s'\n", recv_data.app_name);
			char *log = "; git log --pretty=format:%H -n -1)";
			command = calloc(strlen(cd)+strlen(recv_data.path)+strlen(log)+1,sizeof(char));
			strcat (command, cd);
			strcat (command, recv_data.path);
			strcat (command, log);
			send_result (new_fd, command);
		break;
	    /*Actualizar*/
	    case 2:
			printf ("Actualización de la aplicación: '%s'\n", recv_data.app_name);
			char *pull = "; git pull)";
			command = calloc(strlen(cd)+strlen(recv_data.path)+strlen(pull)+1,sizeof(char));
			strcat (command, cd);
			strcat (command, recv_data.path);
			strcat (command, pull);
			send_result (new_fd, command);
		break;
	    /*Eliminar*/
	    case 3:
			printf ("Se eliminará la aplicación: '%s' instalada en: '%s' \n", recv_data.app_name, recv_data.path);
			char *rm = "rm -fr ";
			command = calloc(strlen(rm)+strlen(recv_data.path)+1, sizeof(char));
			strcat (command, rm);
			strcat (command, recv_data.path);
			strcat (command, "/");
			system(command);
			/*Verifica si existe el directorio*/
			if ( access(recv_data.path, F_OK) != 0 )
			    strcpy (result, "OK\n");
			else
			    strcpy (result, "ERROR");
			send (new_fd, &result,(sizeof(char) * DATASIZE), 0);
		break;
	    /*Instalar*/
	    case 4:
			printf ("Se instalará la aplicación: '%s' en el PATH: '%s'\n", recv_data.app_name, recv_data.path);
			char *clone = "git clone https://github.com/";
			command = calloc(strlen(clone)+strlen(recv_data.user)+strlen(recv_data.repo) +strlen(recv_data.branch)+strlen(recv_data.path)+3, sizeof(char));
			strcat (command, clone);
			strcat (command, recv_data.user);
			strcat (command, "/");
			strcat (command, recv_data.repo);
			strcat (command, " ");
			strcat (command, recv_data.path);
			strcat (command, " -b ");
			strcat (command, recv_data.branch);
			printf("%s\n", command);
			send_result (new_fd, command);
		break;
	}

	/*Cierra la conexión recibida*/
	close (new_fd);
    }
    /*Cierra el socket*/
    close (sockfd);
}
