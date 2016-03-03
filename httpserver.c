/* 
This code primarily comes from 
http://www.prasannatech.net/2008/07/socket-programming-tutorial.html
and
http://www.binarii.com/files/papers/c_sockets.txt
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "httpserver.h"

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int quit;
char *temperature;

void *get_temp(void *args){
  int index;
  int bytes_read;
  int fd = open("/dev/cu.usbmodem1411", O_RDONLY);
  if (fd == -1){
      printf("error accessing usb\n");
      return 9;
  }
  struct termios options;       // struct to hold options
  tcgetattr(fd, &options);      // associate with this fd
  cfsetispeed(&options, 9600);  // set input baud rate
  cfsetospeed(&options, 9600);  // set output baud rate
  tcsetattr(fd, TCSANOW, &options);  // set options
  
  char buff[100];
  
  while(1){
      
      // clearbuff();
      bytes_read = read(fd, buff, 100);
      while(buff[bytes_read-1] != '\n'){
          bytes_read += read(fd, &buff[bytes_read], 100);
         
      }
      //msg will be a global variable
      temperature = malloc(strlen(buff) + 1);
      strcpy(temperature, buff);
  }
}

/* if the user enters 'q', it sets the global value quit to true */
void *stop_server(void *args) {
  quit = 0;
  char input_buffer[255];

  do {
    scanf("%s", input_buffer);
    if (input_buffer[0] == 'q') { /* q to quit */
      pthread_mutex_lock(&lock);
      quit = 1;
      pthread_mutex_unlock(&lock); 
    }
  } while (input_buffer[0] != 'q');

  return NULL;
}

void *start_server(void *args)
{
  /* function arguments */
  struct Server_args *arguments = args;
  const int PORT_NUMBER = arguments->port_number;
  const char *root_dir = arguments->root_dir;

  // structs to represent the server and client
  struct sockaddr_in server_addr,client_addr;    
  
  int sock; // socket descriptor

  // 1. socket: creates a socket descriptor that you later use to make other system calls
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("Socket");
    exit(1);
  }
  int temp;
  if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&temp,sizeof(int)) == -1) {
    perror("Setsockopt");
    exit(1);
  }

  // configure the server
  server_addr.sin_port = htons(PORT_NUMBER); // specify port number
  server_addr.sin_family = AF_INET;         
  server_addr.sin_addr.s_addr = INADDR_ANY; 
  bzero(&(server_addr.sin_zero),8); 
  
  // 2. bind: use the socket and associate it with the port number
  if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
    perror("Unable to bind");
    exit(1);
  }

  // 3. listen: indicates that we want to listen to the port to which we bound; second arg is number of allowed connections
  if (listen(sock, 1) == -1) {
    perror("Listen");
    exit(1);
  }
      
  // once you get here, the server is set up and about to start listening
  printf("\nServer configured to listen on port %d\n", PORT_NUMBER);
  fflush(stdout);
 
  /* constant string for response headers */
  char *header_default = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
  char *header_404 = "HTTP/1.1 404 Not Found\n\n";
  char *header_500 = "HTTP/1.1 500 Internal Server Error\n\n";
  
  /* counters for the stats page */
  int successful_req = 0;
  int failed_req = 0;
  int successful_bytes_sent = 0;

  /* head node of recent retrievals linked list */
  struct Recent_files *head_node = NULL;

  /* loop for each request to server */ 
  while(1) {

    /* if the user entered q, gracefully quit after last request */
    pthread_mutex_lock(&lock);
    if (quit) {
      break;
    }
    pthread_mutex_unlock(&lock); 

    /* by default, the response header is 200 */
    char *header = header_default;

    // 4. accept: wait here until we get a connection on that port
    int sin_size = sizeof(struct sockaddr_in);
    int fd = accept(sock, (struct sockaddr *)&client_addr,(socklen_t *)&sin_size);
    if (fd != -1) {
      printf("Server got a connection from (%s, %d)\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
    
      // buffer to read data into
      char request[1024];
      
      // 5. recv: read incoming message into buffer
      int bytes_received = recv(fd,request,1024,0);
      request[bytes_received] = '\0';  // null-terminate the string
      
      /* parse the request to identify file to send to client */
      strtok(request, " "); // get a pointer to the first '/' char in request
      char *rel_filepath = strtok(NULL, " ");

      /* ignore request for favicon */
      if (strcmp(rel_filepath, "/favicon.ico") == 0) {
        goto CLOSE_SOCKET;
      }

      /* arduino temp page */
      if (strcmp(rel_filepath, "/stats") == 0) {
        
        char stats_buffer[255];

        sprintf (stats_buffer, "Number of page requests handled succesfully: %d\n", successful_req);
        send(fd, stats_buffer, strlen(stats_buffer), 0);

        sprintf (stats_buffer, "Number of page requests that could not be handled: %d\n", failed_req);
        send(fd, stats_buffer, strlen(stats_buffer), 0);

        sprintf (stats_buffer, "Total number of bytes sent back to the client for all successful page requests: %d\n", successful_bytes_sent);
        send(fd, stats_buffer, strlen(stats_buffer), 0);

        sprintf (stats_buffer, "Files retrieved for all successful page requests:\n");
        send(fd, stats_buffer, strlen(stats_buffer), 0);
        
        /* print linked list of recent files */
        struct Recent_files *current = head_node;
        while (current) {
          send(fd, current->name, strlen(current->name), 0);
          sprintf (stats_buffer, "\n");
          send(fd, stats_buffer, strlen(stats_buffer), 0);
          current = current->next;
        }

        goto CLOSE_SOCKET;
      }

      /* stats page */
      if (strcmp(rel_filepath, "/temp") == 0) {
        
        send(fd, temperature, strlen(temperature), 0);

        goto CLOSE_SOCKET;
      }

      /* concatenate the root directory and the relative file path into an absolute file path */
      char *abs_filepath = malloc(strlen(root_dir) + strlen(rel_filepath) + 1);
      if (abs_filepath == NULL) { // failed malloc 500 error

        header = header_500;
        send(fd, header, strlen(header), 0);
        failed_req++;

        goto CLOSE_SOCKET;
      }
      strcpy(abs_filepath, root_dir);
      strcat(abs_filepath, rel_filepath);

      /* open the file requested */
      FILE *file = fopen(abs_filepath, "r");
      free(abs_filepath);

      /* 404 error if file cannot be found/opened */
      if (file == NULL) {

        header = header_404; // set the header to 404
        send(fd, header, strlen(header), 0); 
        
        /* display message that the requested file could not be found */
        char *not_found_msg = "Sorry, could not find ";
        send(fd, not_found_msg, strlen(not_found_msg), 0); 
        send(fd, rel_filepath, strlen(rel_filepath), 0); 
        failed_req++;
        fclose(file);

        goto CLOSE_SOCKET;

      } else {

        /* send 200 header if it opens */
        char file_buffer[1024];          
        successful_bytes_sent += send(fd, header, strlen(header), 0);
        /* send contents of the file */
        while(fgets(file_buffer, sizeof(file_buffer), file)) {
          successful_bytes_sent += send(fd, file_buffer, strlen(file_buffer), 0);
        }

        /* if the request is not already in the list, add a new node */
        if (!is_in_list(head_node, rel_filepath)) {
          
          /* create a new list if there is currently no head node */
          if (head_node == NULL) {

            head_node = malloc(sizeof(struct Recent_files));
            head_node->next = NULL;

          } else {

          /* otherwise, add a new node to the head of the list */
            struct Recent_files *prev_head;
            prev_head = head_node;
            head_node = malloc(sizeof(struct Recent_files));
            head_node->next = prev_head;

          }

          /* set file name to new node in list */
          head_node->name = malloc(strlen(rel_filepath) + 1);
          if (head_node->name == NULL) { // failed malloc 500 error
            
            header = header_500;
            send(fd, header, strlen(header), 0);
            failed_req++;
            fclose(file);

            goto CLOSE_SOCKET;
          }
          strcpy(head_node->name, rel_filepath);

        }

        successful_req++;
        
      }
    }  

    /* if a 404/500 error occurs, program goes right to this label */
    CLOSE_SOCKET:
      close(fd);
      printf("Server closed connection\n");
  }

  // 8. close: close the socket
  close(sock);

  /* free all the nodes for the linked list */
  struct Recent_files *next_node;
  while(head_node) {
    next_node = head_node->next;
    free(head_node->name);
    free(head_node);
    head_node = next_node;
  }

  printf("Server shutting down\n");

  return NULL;
} 

int main(int argc, char *argv[])
{
  // check the number of arguments
  if (argc != 3) {
      printf("\nUsage: %s [port_number] [root_dir]\n", argv[0]);
      exit(-1);
  }

  int port_number = atoi(argv[1]);
  if (port_number <= 1024) {
    printf("\nPlease specify a port number greater than 1024\n");
    exit(-1);
  }

  pthread_t t_start, t_stop, t_temp;
  struct Server_args server_args;

  /* initialize arguments to pass to start_server in a struct */
  server_args.port_number = port_number;
  server_args.root_dir = argv[2];

  /* create thread for start_server and for stop_server */
  int err;
  err = pthread_create(&t_start, NULL, start_server, &server_args);
  if (err != 0)
    printf("Can't create thread: %s\n", strerror(err));
  
  /* quit server thread */
  printf("Enter 'q' to stop server: ");
  err = pthread_create(&t_stop, NULL, stop_server, NULL);
  if (err != 0)
    printf("Can't create thread: %s\n", strerror(err));
  
  /* start serial connection thread */
  err = pthread_create(&t_temp, NULL, get_temp, NULL);
  if (err != 0)
    printf("Can't create thread: %s\n", strerror(err));
  
  /* wait for threads to finish */
  err = pthread_join(t_start, NULL);
  if (err != 0)
    printf("Can't create thread: %s\n", strerror(err));
  err = pthread_join(t_stop, NULL);
  if (err != 0)
    printf("Can't create thread: %s\n", strerror(err));
  err = pthread_join(t_temp, NULL);
  if (err != 0)
    printf("Can't create thread: %s\n", strerror(err));

  return 0;
}
