void* serial_connection_thread(void* arg);
void *stop_server(void *args);
void *start_server(void *args);


/* given a head node, checks if a file is already in the list */
/* returns 1 for true, 0 for false */
int is_in_list(struct Recent_files *head_node, const char *name) {

  while (head_node) {

    if (strcmp(head_node->name, name) == 0) {
      return 1;
    }

    head_node = head_node->next;

  }

  return 0;
}

/* struct to pass in arguments to start_server function */
struct Server_args {
    int port_number;
    char *root_dir;
};

/* linked list node to store all files sent succesfully */
struct Recent_files {
  char *name;
  struct Recent_files *next;
};