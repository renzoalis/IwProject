#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <jansson.h>
#include <curl/curl.h>

#define MAXAPP 15		//Número máximo de aplicaciones
#define MAXSERVER 20		//Número máximo de servidores
#define DATASIZE 1024		//Tamaño máximo del mensaje recibido

#define BUFFER_SIZE  (128 * 1024)  //Buffer para recibir datos de GitHub API
#define URL_FORMAT   "https://api.github.com/repos/%s/%s/compare/%s...%s"
#define URL_SIZE     256

/*------------------------------------------------------------+
 + 		    	   ESTRUCTURAS			      +
 +------------------------------------------------------------+
 + 'sv': Almacena los datos de un servidor                    +
 + 'app': Almacena los datos de una aplicación. Define un     +
 +        array de servidores                                 +
 + 'data': Almacena los datos del mensaje a enviar            +
 + 'write_result': estructura auxiliar para leer datos	      +
 +  				de GitHub		      +
 +-----------------------------------------------------------*/
typedef struct {
	char sv_name[15];
	char address[15];
	char port[10];
	char path[100];
} sv;

typedef struct {
	char app_name[50];
	char repo[50];
	char user[50];
	char branch[50];
	int total_sv;
	sv app_sv[MAXSERVER];
} app;

typedef struct {
	char app_name[50];
	char repo[50];
	char user[50];
	char branch[50];
	char path[100];
	int option;
} data;

struct write_result
{
    char *data;
    int pos;
};

/*--------------------------------------------------------------------------------+
 +			              FUNCIONES			                  +
 +-------------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------------+
 + NOMBRE: 'read_json_app'				                          +
 + OBJETIVO: Leer un archivo JSON - Alamacenar los datos en un array              +
 + PARÁMETROS: app_array (array de aplicaciones)                                  +
 + DESCRIPCIÓN: Leer el archivo JSON donde se encuentran definidas las aplic.     +
 +              disponibles, junto con sus correspondientes servidores.           +
 +-------------------------------------------------------------------------------*/
int read_json_app(app app_array[MAXAPP])
{
	size_t i, j;
	json_t *root;
	json_error_t error;

	/*Cargar el archivo: en caso de error, envía el resultado a 'json_error_t'*/
	root = json_load_file("./app_info.json", 0, &error);
	if (!root) {
		fprintf(stderr, "ERROR: on line %d: %s\n", error.line, error.text);
		return 1;
	}

	/*Verifico que el valor de retorno sea un array*/
	if (!json_is_array(root)) {
		fprintf(stderr, "ERROR: root is not an array\n");
		json_decref(root);
		return 1;
	}

	/*Recorrer las aplicaciones*/
	for (i = 0; i < json_array_size(root); i++)
	{
		json_t *app_data, *app_name, *repo, *user, *branch, *servers;
		app_data = json_array_get(root, i);

		/*Leer los objetos*/
		app_name = json_object_get(app_data, "app_name");
		repo = json_object_get(app_data, "app_repos");
		user = json_object_get(app_data, "app_user");
		branch = json_object_get(app_data, "app_branch");
		servers = json_object_get(app_data, "sv_apps");

		/*Recorrer los servidores de una aplicación*/
		for (j = 0; j < json_array_size(servers); j++)
		{
			json_t *sv_data, *sv_name, *address, *port, *path;
			sv_data = json_array_get(servers, j);

			/*Leer los objetos*/
			sv_name = json_object_get(sv_data, "sv_name");
			address = json_object_get(sv_data, "sv_address");
			port = json_object_get(sv_data, "sv_port");
			path = json_object_get(sv_data, "sv_path");

			/*Asignar los datos al array*/
			strcpy(app_array[i].app_sv[j].sv_name, json_string_value(sv_name));
			strcpy(app_array[i].app_sv[j].address, json_string_value(address));
			strcpy(app_array[i].app_sv[j].port, json_string_value(port));
			strcpy(app_array[i].app_sv[j].path, json_string_value(path));
		}
		strcpy(app_array[i].app_name, json_string_value(app_name));
		strcpy(app_array[i].repo, json_string_value(repo));
		strcpy(app_array[i].user, json_string_value(user));
		strcpy(app_array[i].branch, json_string_value(branch));
		app_array[i].total_sv = json_array_size(servers);

		/*libero el recurso 'servers'*/
		json_decref(servers);
	}
	/*retorno la cantidad total de aplicaciones en el archivo*/
	return (json_array_size(root));
	json_decref(root);
}


/*--------------------------------------------------------------------------------+
 + NOMBRE: 'read_json_sv'                                                         +
 + OBJETIVO: Leer un archivo JSON - Almacenar los datos en un array               +
 + PARÁMETROS: sv_array (array de servidores)                                     +
 + DESCRIPCIÓN: Leer el archivo JSON donde se encuentran almacenados todos los    +
 +              servidores disponibles                                            +
 +-------------------------------------------------------------------------------*/

int read_json_sv(sv sv_array[MAXSERVER])
{
	size_t i;
	json_t *root;
	json_error_t error;

	root = json_load_file("./server_info.json", 0, &error);
	if (!root) {
		fprintf(stderr, "ERROR: on line %d: %s\n", error.line, error.text);
		return 1;
	}

	if (!json_is_array(root)) {
		fprintf(stderr, "ERROR: root is not an array\n");
		json_decref(root);
		return 1;
	}

	for (i = 0; i < json_array_size(root); i++) {
		json_t *sv_data, *name, *address, *port;

		sv_data = json_array_get(root, i);
		name = json_object_get(sv_data, "sv_name");
		address = json_object_get(sv_data, "sv_address");
		port = json_object_get(sv_data, "sv_port");

		strcpy(sv_array[i].sv_name, json_string_value(name));
		strcpy(sv_array[i].address, json_string_value(address));
		strcpy(sv_array[i].port, json_string_value(port));
	}
	return (json_array_size(root));
	json_decref(root);
}


/*--------------------------------------------------------------------------------+
 + NOMBRE: 'add_sv_json_app'                                                      +
 + OBJETIVO: Agregar un servidor del archivo JSON                                 +
 + PARÁMETROS: *server (un servidor), choice (aplicación seleccionada)            +
 + DESCRIPCIÓN: Recibir una aplicación y agregar un nuevo servidor a la misma     +
 +-------------------------------------------------------------------------------*/

void add_sv_json_app(sv *server, int choice)
{
	size_t i;
	json_t *root, *new;
	json_error_t error;

	/*Crear el objeto JSON y asignar los valores correspondientes*/
	new = json_object();
	json_object_set_new(new, "sv_name", json_string(server->sv_name));
	json_object_set_new(new, "sv_address", json_string(server->address));
	json_object_set_new(new, "sv_port", json_string(server->port));
	json_object_set_new(new, "sv_path", json_string(server->path));

	/*Cargar el archivo JSON, buscar y seleccionar la aplicación correspondiente*/
	root = json_load_file("./app_info.json", 0, &error);
	for (i = 0; i < json_array_size(root); i++) {
		json_t *app_data, *servers;
		if (i + 1 == choice) {
			app_data = json_array_get(root, i);
			servers = json_object_get(app_data, "sv_apps");
			/*Agregar el nuevo objeto al final del array*/
			json_array_append_new(servers, new);
		}
	}
	/*Reescribir el archivo - JSON_PRESERVER_ORDER (conserva orden) - JSON_INDENT(4) (conserva estética)*/
	json_dump_file(root, "./app_info.json", (JSON_PRESERVE_ORDER | JSON_INDENT(4)));
	json_decref(root);
}


/*--------------------------------------------------------------------------------+
 + NOMBRE: 'remove_sv_json_app'                                                   +
 + OBJETIVO: Eliminar un servidor del archivo JSON                                +
 + PARÁMETROS: app (aplicación seleccionada) - sv (servidor seleccionado)         +
 + DESCRIPCIÓN: Recibir y eliminar un servidor de la aplicación seleccionada      +
 +-------------------------------------------------------------------------------*/

void remove_sv_json_app(int app, int sv)
{
	size_t i, j;
	json_t *root;
	json_error_t error;
	/*Recorrer las aplicaciones y seleccionar*/
	root = json_load_file("./app_info.json", 0, &error);
	for (i = 0; i < json_array_size(root); i++) {
		json_t *app_data, *servers;

		app_data = json_array_get(root, i);
		if (i + 1 == app) {
			/*Recorrer los servidores y seleccionar*/
			servers = json_object_get(app_data, "sv_apps");
			for (j = 0; j < json_array_size(servers); j++) {
				if (j + 1 == sv)
					/*Eliminar el servidor*/
					json_array_remove(servers, j);
			}
		}
	}
	json_dump_file(root, "./app_info.json", (JSON_PRESERVE_ORDER | JSON_INDENT(4)));
	json_decref(root);
}


/*-------------------------------------------------------------------------------+
 + NOMBRE: 'show_app'                                                            +
 + OBJETIVO: Mostrar el pantalla las aplicaciones                                +
 + PARÁMETROS: app_array (array de aplicaciones) - length (tamaño del array)     +
 + DESCRIPCIÓN: Recibir e imprimir el array de aplicaciones                      +
 +------------------------------------------------------------------------------*/

void show_app(app app_array[MAXAPP], int length)
{
	int i;
	printf("\t\tAPLICACIONES DISPONIBLES\n\n");
	for (i = 0; i < length; i++) {
		printf("%d \tNOMBRE: %s \tREPOSITORIO: %s\n", i + 1, app_array[i].app_name, app_array[i].repo);
	}
	printf("\n0. \tSalir\n");
}

/*-------------------------------------------------------------------------------+
 + NOMBRE: 'show_sv'                                                             +
 + OBJETIVO: Mostrar el pantalla los servidores de una aplicación                +
 + PARÁMETROS: *application (una aplicación)                                     +
 + DESCRIPCIÓN: Recibir e imprimir los servidores de una aplicación              +
 +------------------------------------------------------------------------------*/

void show_sv(app *application)
{
	int i;
	printf("\t\tSERVIDORES DISPONIBLES: %s\n\n", application->app_name);
	for (i = 0; i < application->total_sv; i++) {
		printf("%d \tNOMBRE: %s PATH: %s\n", i + 1, application->app_sv[i].sv_name, application->app_sv[i].path);
	}
	printf("\n%d. \tAgregar servidor\n", i + 1);
	printf("\n0. \tVolver\n");
}


/*--------------------------------------------------------------------------------+
 + NOMBRE: 'show_task'                                                            +
 + OBJETIVO: Mostrar en pantalla las tareas disponibles                           +
 + PARÁMETROS: *application (una aplicación) - *server (un servidor)              +
 +-------------------------------------------------------------------------------*/

void show_task(app *application, sv *server)
{
	printf("\t\tTAREAS DISPONIBLES: APP: %s. SERVER: %s\n\n", application->app_name, server->sv_name);
	printf("1. \tCONSULTAR\n");
	printf("2. \tACTUALIZAR\n");
	printf("3. \tELIMINAR\n");
	printf("\n0. \tVOLVER\n");
}


/*--------------------------------------------------------------------------------+
 + NOMBRE: 'get_app'                                                              +
 + OBJETIVO: Obtener una aplicación                                               +
 + PARÁMETROS: app_array (array de aplicaciones) - *application (una aplicación)  +
 +             length (tamaño del array) - choice (aplicación seleccionada)       +
 + DESCRIPCIÓN: Recibir el array de aplicaciones                                  +
 +              BUscar y retornar la aplicación seleccionada                      +
 +-------------------------------------------------------------------------------*/

void get_app(app app_array[MAXAPP], app *application, int length, int choice)
{
	int i, j;
	for (i = 0; i < length; i++) {
		if (i + 1 == choice) {
			strcpy(application->app_name, app_array[i].app_name);
			strcpy(application->repo, app_array[i].repo);
			strcpy(application->user, app_array[i].user);
			strcpy(application->branch, app_array[i].branch);
			application->total_sv = app_array[i].total_sv;

			for (j = 0; j < application->total_sv; j++) {
				strcpy(application->app_sv[j].sv_name, app_array[i].app_sv[j].sv_name);
				strcpy(application->app_sv[j].address, app_array[i].app_sv[j].address);
				strcpy(application->app_sv[j].port, app_array[i].app_sv[j].port);
				strcpy(application->app_sv[j].path, app_array[i].app_sv[j].path);
			}
		}
	}
}


/*--------------------------------------------------------------------------------+
 + NOMBRE: 'get_sv'                                                               +
 + OBJETIVO: Obtener un servidor de una aplicación                                +
 + PARÁMETROS: *application (una aplicación) - *server (un servidor) -            +
 +             choice (servidor seleccionado)                                     +
 + DESCRIPCIÓN: Recibir una aplicación                                            +
 +              Buscar y retornar el servidor seleccionado de la aplicación       +
 +-------------------------------------------------------------------------------*/

void get_sv(app *application, sv *server, int choice)
{
	int i;
	for (i = 0; i < application->total_sv; i++) {
		if (i + 1 == choice) {
			strcpy(server->sv_name, application->app_sv[i].sv_name);
			strcpy(server->address, application->app_sv[i].address);
			strcpy(server->port, application->app_sv[i].port);
			strcpy(server->path, application->app_sv[i].path);
		}
	}
}


/*--------------------------------------------------------------------------------+
 + NOMBRE: 'get_data'                                                             +
 + OBJETIVO: Obtener el mensaje                                                   +
 + PARÁMETROS: *message (un mensaje) - *application (una aplicación) -            +
 +             *server (un servidor) - task (tarea a realizar)                    +
 + DESCRIPCIÓN: Recibir una aplicación y servidor seleccionado. Armar el mensaje  +
 +              con los campos correspondientes                                   +
 +-------------------------------------------------------------------------------*/

data get_data(app *application, sv *server, int task)
{
	data message;

	strcpy(message.app_name, application->app_name);
	strcpy(message.repo, application->repo);
	strcpy(message.user, application->user);
	strcpy(message.branch, application->branch);
	strcpy(message.path, server->path);
	message.option = task;

	return message;
}


/*--------------------------------------------------------------------------------+
 + NOMBRE: 'send_request'                                                         +
 + OBJETIVO: Enviar mensaje a 'app_client'                                        +
 + PARÁMETROS: *server (un servidor) - message (un mensaje)                       +
 + DESCRIPCIÓN: Inicializar las funciones de la biblioteca BSD Socket             +
 +              Enviar el mensaje al servidor correspondiente. Será 'app_client'  +
 +              quién se encargrá de recibir dicho paquete en el servidor. Como   +
 +              valor de retorno se obtiene el resultado de la ejecución          +
 +-------------------------------------------------------------------------------*/

char * send_request(sv *server, data message)
{
	/*Donde se recibe el resultado*/
	char * result = malloc(sizeof(char) * DATASIZE);

	/*Variables socket*/
	int sockfd, n;
	struct hostent *he = malloc(sizeof(struct hostent));
	struct sockaddr_in sv_addr;

	/*Obtener la dirección del servidor*/
	if ((he = gethostbyname(server->address)) == NULL) {
		perror("\nERROR: Dirección no válida");
		exit(1);
	}

	/*Inicializar socket*/
	if ((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		perror("socket");
		exit(1);
	}

	/*Completar la información del servidor*/
	sv_addr.sin_family = AF_INET;
	sv_addr.sin_port = htons(atoi(server->port));
	sv_addr.sin_addr = *((struct in_addr*) he->h_addr);
	memset(&(sv_addr.sin_zero), '\0', 8);

	/*Conectar al servidor*/
	if (connect(sockfd, (struct sockaddr*) &sv_addr, sizeof(struct sockaddr)) == -1) {
		perror("\nERROR: Conexión no disponible en este momento");
		exit(1);
	}

	/*Enviar el mensaje a 'app_client'*/
	send(sockfd, &message, sizeof(data), 0);

	/*Recibir la respuesta de 'app_client'*/
	if ((n = recv(sockfd, result, DATASIZE, 0)) == -1) {
		perror("recv");
		exit(1);
	}
	if (n < DATASIZE)
		result[n] = '\0';
	else
		result[DATASIZE] = '\0';

	/*Cerrar socket*/
	close(sockfd);

	return result;
}


/*--------------------------------------------------------------------------------+
 + NOMBRE: 'add_sv_to_app'                                                        +
 + OBJETIVO: Instalar una aplicación en un servidor                               +
 + PARÁMETROS: *application (una aplicación) - choice (aplicación seleccionada)   +
 + DESCRIPCIÓN: Seleccionar un servidor donde se instalará la aplicación          +
 +              Si el servidor ya tiene la aplicación, se mostrará dicho resultado+
 +              Caso contrario se agregará un nuevo servidor al array de la       +
 +              aplicación. Finalmente se envía el mensaje a 'app_cliente'        +
 +-------------------------------------------------------------------------------*/

int add_sv_to_app(app *application, int choice)
{
	int length, opt, isInt, i, value, num;
	char path[100];
	sv sv_array[MAXSERVER];

	/*Leer el JSON, para ver todos los servidores disponibles*/
	length = read_json_sv(sv_array);
	system("clear");
	printf("\tServidores disponibles:\n\n");
	for (i = 0; i < length; i++) {
		printf("%d. \tSERVER: %s, PORT: %s\n", i + 1, sv_array[i].sv_name, sv_array[i].port);
	}
	printf("\n0. \tCancelar\n\n");
	printf("Seleccione un servidor: ");
	isInt = scanf("%d", &opt);

	/*VALIDAR OPCIONES*/
	value = 1;
	/*Si la aplicación no esta instalada en ningún servidor*/
	if (application->total_sv == 0) value = 0;

	while (value == 1 || opt < 0 || opt > length) {
		/*Si la opción es incorrecta*/
		if (opt < 0 || opt > length) {
			printf("Opción incorrecta. Intente nuevamente: ");
			isInt = scanf("%d", &opt);
			if (!isInt) return 1;
		}
		/*Recorrer la aplicación y verificar la existencia del servidor seleccionado*/
		else {
			for (i = 0; i < application->total_sv; i++) {
				/*Si el servidor ya tiene la aplicación*/
				if (strcmp(application->app_sv[i].sv_name, sv_array[opt - 1].sv_name) == 0) {
					printf("La aplicación '%s' ya se encuentra instalada en el servidor '%s'\n", application->app_name, application->app_sv[i].sv_name);
					printf("Ingrese otro servidor: ");
					isInt = scanf("%d", &opt);
					value = 1;
					if (!isInt) return 1;
					break;
				}
				else
					value = 0;
			}
		}
	}

	if (opt != 0) {
		sv *server = malloc(sizeof(sv));
		data message;
		char * response;

		printf("\nServidor seleccionado: %s\n", sv_array[opt - 1].sv_name);
		printf("Ingrese el PATH donde desea instalar '%s': ", application->app_name);
		scanf("%s", path);

		/*Armar el nuevo servido*/
		strcpy(server->sv_name, sv_array[opt - 1].sv_name);
		strcpy(server->address, sv_array[opt - 1].address);
		strcpy(server->port, sv_array[opt - 1].port);
		strcpy(server->path, path);

		message = get_data(application, server, 4);
		response = send_request(server, message);
		printf("\nEsta tarea puede durar varios minutos. Espere por favor...\n\n");
		printf("RESULTADO: %s\n", response);
		add_sv_json_app(server, choice);

		printf("\nPresione un 'número' para continuar: ");
		scanf("%d", &num);
	}

	return 0;
}


/*--------------------------------------------------------------------------------+
 +                               WRITE RESPONSE                                   +
 + Utilizada para comparar commits utilizando GitHub API	               		  +
 + Escribe response en buffer 							  						  +
 + Requerida por libcurl                                			  			  +
 +-------------------------------------------------------------------------------*/

static size_t write_response(void *ptr, size_t size, size_t nmemb, void *stream)
{
    struct write_result *result = (struct write_result *)stream;

    if(result->pos + size * nmemb >= BUFFER_SIZE - 1)
    {
        fprintf(stderr, "error: too small buffer\n");
        return 0;
    }

    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}


/*--------------------------------------------------------------------------------+
 +                               REQUEST	                            	      +
 + Utilizada para comparar commits utilizando GitHub API			 			  +
 + Realiza el request a la API de GitHub 					  					  +
 +-------------------------------------------------------------------------------*/

static char *request(const char *url)
{
    CURL *curl = NULL;
    CURLcode status;
    struct curl_slist *headers = NULL;
    char *data = NULL;
    long code;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(!curl)
        goto error;

    data = malloc(BUFFER_SIZE);
    if(!data)
        goto error;

    struct write_result write_result = {
        .data = data,
        .pos = 0
    };

    curl_easy_setopt(curl, CURLOPT_URL, url);

    headers = curl_slist_append(headers, "User-Agent: Jansson Tutorial");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &write_result);

    status = curl_easy_perform(curl);
    if(status != 0)
    {
        fprintf(stderr, "error: unable to request data from %s:\n", url);
        fprintf(stderr, "%s\n", curl_easy_strerror(status));
        goto error;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    if(code != 200)
    {
        fprintf(stderr, "error: server responded with code %ld\n", code);
        goto error;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    curl_global_cleanup();

    /* zero-terminate the result */
    data[write_result.pos] = '\0';

    return data;

error:
    if(data)
        free(data);
    if(curl)
        curl_easy_cleanup(curl);
    if(headers)
        curl_slist_free_all(headers);
    curl_global_cleanup();
    return NULL;
}


//Funcion auxiliar para hacer un strim de cualquir char*
char *trimwhitespace(char *str)
{
  char *end;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = 0;

  return str;
}


/*--------------------------------------------------------------------------------+
 +                               GITHUB_COMPARE_COMMITS	                          +
 + Utilizada para comparar commits utilizando GitHub API			  +
 + Obtiene cuantos commits 'atras' se encuentra 'sha' con respecto a 'branch'	  +
 +-------------------------------------------------------------------------------*/

int github_compare_commits(char *usuario, char *repo, char *branch, char *sha) {
    char *text;
    char url[URL_SIZE];

    json_t *root;
    json_error_t error;

    snprintf(url, URL_SIZE, URL_FORMAT, trimwhitespace(usuario), trimwhitespace(repo), trimwhitespace(branch), trimwhitespace(sha));
    text = request(url);
    if(!text)
        return 1;

    root = json_loads(text, 0, &error);
    free(text);

    if(!root)
    {
        fprintf(stderr, "error: on line %d: %s\n", error.line, error.text);
        return 1;
    }

	json_t *behind_by;

	if(!json_is_object(root))
	{
		fprintf(stderr, "error: response data is not an object\n");
		json_decref(root);
		return 1;
	}

	behind_by = json_object_get(root, "behind_by");
	if(!json_is_integer(behind_by)) 
	{
		fprintf(stderr, "error: behind_by is not a integer\n");
		return 1;
	}

	int result;
	result = (int)json_integer_value(behind_by);
	json_decref(root);
	
	return result;
}


/*--------------------------------------------------------------------------------+
 +                               MÓDULO PRINCIPAL                                 +
 + OBJETIVO: Realizar la ejecución principal de 'app_server'                      +
 + DESCRIPCIÓN: Mostrar las diferentes pantallas y llamar a las funciones         +
 +              correspondientes                                                  +
 +-------------------------------------------------------------------------------*/
int main()
{
	system("clear");

	/*Variables generales*/
	int option, tmp_opt1, tmp_opt2, len1, len2, isInt;
	app app_array[MAXAPP];
	sv *server = malloc(sizeof(sv));
	app *application = malloc(sizeof(app));
	data message;
	char *response;

	/*Leer el JSON de aplicaciones*/
	len1 = read_json_app(app_array);
	/*Mostrar aplicaciones*/
	show_app(app_array, len1);

	printf("\nSeleccione una aplicación: ");
	isInt = scanf("%d", &option);

	while (option != 0) {
		/*Guardar la opción de aplicación seleccionada*/
		tmp_opt1 = option;

		if (!isInt) return 1;
		if (option > 0 && option <= len1) {
			system("clear");
			/*Obtener la aplicación seleccionada*/
			get_app(app_array, application, len1, option);
			len2 = application->total_sv;
			/*Mostrar servidores de la aplicación*/
			show_sv(application);

			printf("\nSeleccione un servidor: ");
			isInt = scanf("%d", &option);

			while (option != 0) {
				/*Guardar la opción de servidor seleccionado*/
				tmp_opt2 = option;

				if (!isInt) return 1;
				/*Si la opción es 'agregar servidor'*/
				if (option == len2 + 1) {
					add_sv_to_app(application, tmp_opt1);
					len1 = read_json_app(app_array);
					break;
				}

				/*Si se selecciona un servidor*/
				if (option > 0 && option <= len2) {
					system("clear");
					/*Obtener el servidor seleccionado*/
					get_sv(application, server, option);
					/*Mostrar tareas*/
					show_task(application, server);

					printf("\nSeleccione una tarea: ");
					isInt = scanf("%d", &option);

					while (option != 0) {
						if (!isInt) return 1;
						switch (option) {
							/*Consultar*/
						case 1:
							printf("\nAPP: %s\n", application->app_name);
							printf("SERVER: %s\n", server->sv_name);
							printf("TAREA: Consultar último commit de la aplicación\n");
							message = get_data(application, server, option);
							response = send_request(server, message);

							int behind;
							behind = github_compare_commits(application->user, application->repo, application->branch, response);
							if (behind == 0)
								printf("RESULTADO: Repositorio actualizado\n");
							else
								printf("RESULTADO: Repositorio desactualizado. Atras por %d commits.\n", behind);
							break;
							/*Actualizar*/
						case 2:
							printf("\nAPP: %s\n", application->app_name);
							printf("SERVER: %s\n", server->sv_name);
							printf("TAREA: Actualizar aplicación\n");
							message = get_data(application, server, option);
							response = send_request(server, message);
							printf("RESULTADO: %s\n", response);
							break;
							/*ELiminar*/
						case 3:
							printf("\nAPP: %s\n", application->app_name);
							printf("SERVER: %s\n", server->sv_name);
							printf("TAREA: Desinstalar aplicación\n");
							message = get_data(application, server, option);
							response = send_request(server, message);
							printf("RESULTADO: %s\n", response);
							remove_sv_json_app(tmp_opt1, tmp_opt2);
							len1 = read_json_app(app_array);
							break;
						}
						printf("\nSeleccione una tarea: ");
						isInt = scanf("%d", &option);
					}
				}
				system("clear");
				show_sv(application);
				printf("\nSeleccione un servidor: ");
				isInt = scanf("%d", &option);
			}
		}
		system("clear");
		show_app(app_array, len1);
		printf("\nSeleccione una aplicación: ");
		isInt = scanf("%d", &option);
	}
	system("clear");

	return 0;
}
