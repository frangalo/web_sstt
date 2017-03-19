#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <linux/limits.h>

#define VERSION		24
#define BUFSIZE		8096
#define ERROR		42
#define LOG			44
#define PROHIBIDO	403
#define NOENCONTRADO	404
#define NOACEPTADO	406
#define MUCHASPETICIONES	429
#define NOIMPLEMENTADO	501
#define MAXACCESS	5
//
// RUTAS HASTA LOS FICHEROS CON LAS WEBS DE ERROR
//
static char F_PROHIBIDO[] = "pages/forbidden.html";
static char F_NOENCONTRADO[] = "pages/notfound.html";
static char F_NOACEPTADO[] = "pages/notacceptable.html";
static char F_MUCHASPETICIONES[] = "pages/manyrequest.html";
static char F_NOIMPLEMENTADO[] = "pages/notimplemented.html";

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpg" },
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"ico", "image/ico" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{0,0} };


struct HTTPrequest {
	char *method;
	char *url;
	char *version;
};

void debug(int log_message_type, char *message, char *additional_info, int socket_fd)
{
	int fd ;
	char logbuffer[BUFSIZE*2];
	
	switch (log_message_type) {
		case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",message, additional_info, errno,getpid());
			break;
		case PROHIBIDO:
			// Enviar como respuesta 403 Forbidden
			(void)sprintf(logbuffer,"FORBIDDEN: %s:%s",message, additional_info);
			break;
		case NOENCONTRADO:
			// Enviar como respuesta 404 Not Found
			(void)sprintf(logbuffer,"NOT FOUND: %s:%s",message, additional_info);
			break;
		case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",message, additional_info, socket_fd); break;
	}

	if((fd = open("webserver.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer));
		(void)write(fd,"\n",1);
		(void)close(fd);
	}
	if(log_message_type == ERROR || log_message_type == NOENCONTRADO || log_message_type == PROHIBIDO) exit(3);
}


int getFileType(char* path) {
	// obtenemos la extension del archivo.
    char *extension = strrchr(path, '.');
    extension = extension+1; // le quitamos el punto de ".ext"

    // Sacamos el numero de extensiones, que serán 10, el {0,0} no es necesario comprobarlo
    int nExt = (sizeof(extensions)/sizeof(extensions[0].ext))/2 - 1;

 	int indexType = -1;
    // Recorremos el array para saber si es valido el tipo de archivo
    for (int i = 0; i < nExt; ++i) {
    	if ( strcmp(extension, extensions[i].ext) == 0 ) {
    		indexType = i;
    	}
    }

	return indexType;
}


// Parsea la primera línea del mensaje recibido.
void parse_line(char *message, struct HTTPrequest *request) {
	char aux[BUFSIZE] = { 0 };
	char *ptr;
	char *save_ptr;

	strcpy(aux, message);

	ptr = strtok_r(message, " ", &save_ptr);
	request->method = ptr;
	
	ptr = strtok_r(NULL, " ", &save_ptr);
	request->url = ptr;

	ptr = strtok_r(NULL, " ", &save_ptr);
	request->version = ptr;
}


int parse_cookie(char *message) {
	// Copiamos el valor del puntero token que tendra el formato "cookie=x\r\n"
	char cookieHeader[BUFSIZE] = { 0 };
	char *ptr;
	char *save_ptr;

	strcpy(cookieHeader, message);

    ptr = strtok_r(cookieHeader, "=\r\n", &save_ptr);	// Contendra la cabecera "cookie".
	ptr = strtok_r(NULL, "=\r\n", &save_ptr);		// Contendra el valor de la cookie.

	int access_counter = *ptr - '0';

	return access_counter;
}


void send_error(int error_type, int descriptorFichero, struct HTTPrequest request, char *path) {
	FILE *file;
	char response[BUFSIZE] = { 0 };
	int indexType = -1;

	//
	// Estructura del fichero
	//
	struct stat fileStat;

	strcat(response, request.version);

	switch (error_type) {
		case PROHIBIDO:
			if ( (stat(F_PROHIBIDO, &fileStat)) < 0 )
				debug(NOENCONTRADO, F_PROHIBIDO, " file or directory not exist.", 0);

			file = fopen(F_PROHIBIDO, "rb");
			strcat(response, " 403 Forbidden\r\n");
			indexType = getFileType(F_PROHIBIDO);
			break;

		case NOENCONTRADO:
			if ( (stat(F_NOENCONTRADO, &fileStat)) < 0 )
				debug(NOENCONTRADO, F_NOENCONTRADO, " file or directory not exist.", 0);

			file = fopen(F_NOENCONTRADO, "rb");
			strcat(response, " 404 Not Found\r\n");
			indexType = getFileType(F_NOENCONTRADO);
			break;

		case NOACEPTADO:
			if ( (stat(F_NOACEPTADO, &fileStat)) < 0 )
				debug(NOENCONTRADO, F_NOACEPTADO, " file or directory not exist.", 0);

			file = fopen(F_NOACEPTADO, "rb");
			strcat(response, " 406 Not Acceptable\r\n");
			indexType = getFileType(F_NOACEPTADO);
			break;

		case MUCHASPETICIONES:
			if ( (stat(F_MUCHASPETICIONES, &fileStat)) < 0 )
				debug(NOENCONTRADO, F_MUCHASPETICIONES, " file or directory not exist.", 0);

			file = fopen(F_MUCHASPETICIONES, "rb");
			strcat(response, " 429 Too Many Request\r\n");
			indexType = getFileType(F_MUCHASPETICIONES);
			break;

		case NOIMPLEMENTADO:
			if ( (stat(F_NOIMPLEMENTADO, &fileStat)) < 0 )
				debug(NOENCONTRADO, F_NOIMPLEMENTADO, " file or directory not exist.", 0);

			file = fopen(F_NOIMPLEMENTADO, "rb");
			strcat(response, " 501 Not Implemented\r\n");
			indexType = getFileType(F_NOIMPLEMENTADO);
			break;
	}

	if (indexType < 0) {
		debug(ERROR, path, " file type not supported.", 0);
	}

	strcat(response, "Content-Type: ");	// Tipo de contenido
	strcat(response, extensions[indexType].filetype);
	strcat(response, "\r\n");

	//
    // Tamaño total del fichero
    //
    char contentlength[128] = { 0 };
    sprintf(contentlength, "Content-Length: %ld\r\n", fileStat.st_size);
    strcat(response, contentlength);

	strcat(response, "Server: web_sstt\r\n");
	strcat(response, "\r\n");
	write(descriptorFichero, response, strlen(response));

	if (file) {
		char bufferFile[BUFSIZE] = { 0 };
		int readbytes;
		//
		// Leemos el archivo
		//
	    while ( (readbytes = fread(bufferFile, 1, BUFSIZE, file)) ) {
	    	write(descriptorFichero, bufferFile, readbytes);
	    	memset(bufferFile, 0, BUFSIZE);
	    }

	    fclose(file);
	} else {
		debug(ERROR, "Can not open file", "", 0);
	}

	close(descriptorFichero);
	//
	// Mostramos en el log el fallo exacto.
	//
	switch (error_type) {
		case PROHIBIDO:
			debug(PROHIBIDO, path, " failed to open file.", 0);
		case NOENCONTRADO:
			debug(NOENCONTRADO, path, " file or directory not exist.", 0);
		case NOACEPTADO:
			debug(ERROR, path, " file type not supported.", 0);
		case MUCHASPETICIONES:
			debug(ERROR, "HTTP 429 error", " too many request.", 0);
		case NOIMPLEMENTADO:
			debug(ERROR, "HTTP 501 error", " method not implemented.", 0);
	}
}


void process_web_request(int descriptorFichero, char *dir)
{
	debug(LOG,"request","Ha llegado una peticion",descriptorFichero);
	//
	// Definir buffer y variables necesarias para leer las peticiones
	//
	char logbuffer[BUFSIZE] = { 0 };
	struct HTTPrequest request;
	int access_counter = 0;
	//
	// Buffer que contendra la respuesta HTTP
	//
	char response[BUFSIZE] = { 0 };

	//
	// Leer la petición HTTP
	//
	ssize_t n = read(descriptorFichero, logbuffer, BUFSIZE);
	//
	// Comprobación de errores de lectura
	//
	if (n < 0) {
		close(descriptorFichero);
		debug(ERROR, "system call","read", 0);
	}
	
	//
	// Obtenemos la informacion del request
	//
	char *token = strtok(logbuffer, "\r\n");	// Primera linea: GET / HTTP/1.1
	parse_line(token, &request);	// La parseamos para guardar los valores
	token = strtok(NULL, "\r\n");	// Siguientes llamadas
	//
	// Siguientes llamadas, lecturas de cabeceras
	//
	while (token) {
		debug(LOG,"\t\tHeader request", token, descriptorFichero);
		//
		// Comprobamos si obtenemos la cabecera de una Cookie
		//
   		if ( strncmp(token, "Cookie", 6) == 0 ) {
   			access_counter = parse_cookie(token);
   		}

   		token = strtok(NULL, "\r\n");
   	}

   	//
   	// Si se han hecho 5 accesos se muestra el error
   	//
   	if (access_counter >= MAXACCESS) {
   		send_error(MUCHASPETICIONES, descriptorFichero, request, NULL);
   	} else access_counter++;
	
	//
	//	TRATAR LOS CASOS DE LOS DIFERENTES METODOS QUE SE USAN
	//	(Se soporta solo GET)
	//
    // Primero comprobamos si es un GET, si no, no soportado
    //
	if ( strcmp(request.method,"GET") == 0 ) {
		//
		// Estructura del fichero
		//
		struct stat fileStat;
		//
		// Creamos la ruta que nos pide el request
		//
		char path[PATH_MAX] = { 0 };
		strcat(path,".");
		strcat(path,request.url);
		//
		// Comprobamos que el archivo o directorio es válido.
		//
		if ( (stat(path, &fileStat)) < 0 ) {
			send_error(NOENCONTRADO, descriptorFichero, request, path);
		}
		//
		// Si es válido entonces seguimos la ejecución. Si es un directorio buscaremos por defecto el fichero index.html
		//
		if ( S_ISDIR(fileStat.st_mode) ) {
           	strcat(path,"/index.html");
           	if ( (stat(path, &fileStat)) < 0 ) {
				send_error(NOENCONTRADO, descriptorFichero, request, path);
			}
        }

        int indexType = getFileType(path);

        // Si no se proporciona un archivo soportado se envía un error.
        if (indexType < 0) {
        	send_error(NOACEPTADO, descriptorFichero, request, path);
        }
        //
        // Compruebo si tengo permisos para abrir el archivo
        //
        if ( access(path, R_OK) != -1 ) {
        	FILE *file = fopen(path, "rb");
	        // Si existe el fichero se crea la respuesta
	        if (file) { 
	        	//
		        // Se construye la respuesta  HTTP
		        //
	        	strcat(response, request.version);	// Version HTTP
				strcat(response, " 200 OK\r\n");		// 200 OK
				//
		        // Tamaño total del fichero
		        //
		        char contentlength[128] = { 0 };
		        sprintf(contentlength, "Content-Length: %ld\r\n", fileStat.st_size);

				strcat(response, "Content-Type: ");	// Tipo de contenido
				strcat(response, extensions[indexType].filetype);
				strcat(response, "\r\n");
				strcat(response, contentlength);		// Tamaño del archivo
				strcat(response, "Server: web_sstt\r\n");
				//
	 	        // Guardamos la fecha y tiempo actual.
	 	        //
		        time_t now = time( NULL );
		        struct tm * now_tm;
		        now_tm = gmtime(&now);

		        char bufferTime[128] = { 0 };
		        strftime(bufferTime, sizeof(bufferTime), "Date: %a, %e %b %Y %T %Z\r\n", now_tm);
		        
		        strcat(response, bufferTime);
		        //
		        // Cookie. Guardamos la fecha futura de cuando expira la cookie.
		        //
		        char cookie[256] = { 0 };
		        
		        memset(bufferTime, 0, BUFSIZE);

		        time_t future = now + 600; // Aumentamos 6 horas a la actual
				struct tm * then_tm;
				then_tm = gmtime(&future);

		        sprintf(cookie, "Set-Cookie: access_counter=%d; ", access_counter);
		        strftime(bufferTime, sizeof(bufferTime), "Expires=%a, %e %b %Y %T %Z\r\n", then_tm);
		        strcat(cookie, bufferTime);
		        strcat(response, cookie);
		        strcat(response, "\r\n");
				
				// Mandamos el HTTP response
		        size_t nbytes = write(descriptorFichero, response, strlen(response));
		        if ( strlen(response) != nbytes ) {
		        	nbytes += write(descriptorFichero, response+nbytes, strlen(response)-nbytes);
		        }
		        //
		        // AHORA SE MANDARAN LOS DATOS
		        //
				// Buffers auxiliares
				//
				char bufferFile[BUFSIZE] = { 0 };
				int readbytes;
				//
				// Leemos el archivo
				//
		        while ( (readbytes = fread(bufferFile, 1, BUFSIZE, file)) ) {
		        	if (readbytes < 0) {
		        		close(descriptorFichero);
		        		debug(ERROR, "Read", "fatal file read", 0);
		        	}

		        	write(descriptorFichero, bufferFile, readbytes);
		        	memset(bufferFile, 0, BUFSIZE);
		        }

		        fclose(file);
	        } 
	        // Si no se ha podido abrir el fichero
	        else send_error(NOENCONTRADO, descriptorFichero, request, path);
        }
        // Si no tengo acceso al fichero (permisos)
        else send_error(PROHIBIDO, descriptorFichero, request, path);
    // si no es un metodo GET
	} else send_error(NOIMPLEMENTADO, descriptorFichero, request, NULL);
	
	close(descriptorFichero);
	exit(1);
}

int main(int argc, char **argv)
{
	int i, port, pid, listenfd, socketfd;
	socklen_t length;
	static struct sockaddr_in cli_addr;		// static = Inicializado con ceros
	static struct sockaddr_in serv_addr;	// static = Inicializado con ceros
	
	//  Argumentos que se esperan:
	//
	//	argv[1]
	//	En el primer argumento del programa se espera el puerto en el que el servidor escuchara
	//
	//  argv[2]
	//  En el segundo argumento del programa se espera el directorio en el que se encuentran los ficheros del servidor
	//
	//  Verficiar que los argumentos que se pasan al iniciar el programa son los esperados
	//
	if(argc != 3) {
		(void)printf("USAGE: ./web_sstt [port] [server_directory]\n");
		exit(2);
	}
	//
	//  Verficiar que el directorio escogido es apto. Que no es un directorio del sistema y que se tienen
	//  permisos para ser usado
	//
	struct stat workspace;
	if ( stat(argv[2],&workspace) < 0) {
		(void)printf("ERROR: No existe el directorio %s\n",argv[2]); 
	    exit(5);
	}

	if ( access(argv[2], W_OK) != 0) {
		(void)printf("ERROR: No se tienen permisos de escritura en el directorio %s\n", argv[2]);
		exit(6);
	}

	if ( chdir(argv[2]) == -1 ){ 
		(void)printf("ERROR: No se puede cambiar de directorio %s\n",argv[2]);
		exit(4);
	}
	// Hacemos que el proceso sea un demonio sin hijos zombies
	if(fork() != 0)
		return 0; // El proceso padre devuelve un OK al shell

	(void)signal(SIGCHLD, SIG_IGN); // Ignoramos a los hijos
	(void)signal(SIGHUP, SIG_IGN); // Ignoramos cuelgues
	
	debug(LOG,"web server starting...", argv[1] ,getpid());
	
	/* setup the network socket */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		debug(ERROR, "system call","socket",0);
	
	port = atoi(argv[1]);

	if (port <= 1024)
		debug(ERROR,"Puerto invalido, prueba un puerto mayor de 1024",argv[1],0);
	if(port < 0 || port >60000)
		debug(ERROR,"Puerto invalido, prueba un puerto de 1 a 60000",argv[1],0);
	
	/*Se crea una estructura para la información IP y puerto donde escucha el servidor*/
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /*Escucha en cualquier IP disponible*/
	serv_addr.sin_port = htons(port); /*... en el puerto port especificado como parámetro*/
	
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		debug(ERROR,"system call","bind",0);
	
	if( listen(listenfd,64) <0)
		debug(ERROR,"system call","listen",0);
	
	while(1){
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			debug(ERROR,"system call","accept",0);
		if((pid = fork()) < 0) {
			debug(ERROR,"system call","fork",0);
		}
		else {
			if(pid == 0) { 	// Proceso hijo
				(void)close(listenfd);
				process_web_request(socketfd, argv[2]); // El hijo termina tras llamar a esta función
			} else { 	// Proceso padre
				(void)close(socketfd);
			}
		}
	}
}
