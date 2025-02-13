#include <stdio.h> // sprintf
#include <stdlib.h> // exit malloc calloc free atoi
#include <strings.h> // bzero
#include <string.h> // strlen strcpy strcat
#include <unistd.h> // write
#include <sys/socket.h> // socket bind
#include <netinet/in.h> // struct sockaddr_in
#include <arpa/inet.h> // htons htonl
#include <sys/select.h> // select

//Types
typedef struct client_s {
	struct client_s*	next;

	char*				buffer;
	int					fd;
	int					index;
}	client;

typedef struct server_s {
	int			fd;
	client*		clients;
	int			next_index;
}	server;
//--


//Utils
void	exit_err(char* err)
{
	write(STDERR_FILENO, err, strlen(err));
	exit(EXIT_FAILURE);
}

char*	str_join(char* buf, char* add)
{
	char*	newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (NULL); // failed system call
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf); // joined
}
//--


//Mini-serv
int extract_message(char** buf, char** msg)
{
	char*	newbuf;
	int		i;

	*msg = 0;
	if (*buf == 0)
		return (0); // nothing to read
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(strlen(*buf + i + 1) + 1, sizeof(*newbuf));
			if (newbuf == 0)
				return (-1); // failed system call
			strcpy(newbuf, *buf + i + 1);

			*msg = *buf;
			(*msg)[i + 1] = 0;

			*buf = newbuf;
			return (1); // msg finished, buffer contains everything after the message (its size >= 1)
		}
		i++;
	}
	return (0); // msg not finished, nothing to do
}

void	build_fd_sets(server* server, fd_set* read, fd_set* write)
{
	FD_ZERO(read);
	FD_ZERO(write);

	FD_SET(server->fd, read);

	client* clients;
	for (clients = server->clients; clients != NULL; clients = clients->next) {
		FD_SET(clients->fd, read);
		FD_SET(clients->fd, write);
	}
}

void	push_client(client** clients, client* new, int index)
{
	new->buffer = NULL;
	new->next = NULL;
	new->index = index;

	if (*clients == NULL) {
		*clients = new;
		return;
	}

	client* last = *clients;
	while (last->next != NULL)
		last = last->next;

	last->next = new;
}

void	pop_client(client** clients, int client_fd)
{
	if (*clients == NULL)
		return;
	if ((*clients)->fd == client_fd) {
		client* next = (*clients)->next;
		close((*clients)->fd);
		free((*clients)->buffer);
		free(*clients);
		*clients = next;
		return;
	}

	client* pre_pop = *clients;
	client* pop = pre_pop->next;
	while (pop != NULL) {
		if (pop->fd == client_fd) {
			pre_pop->next = pop->next;
			close(pop->fd);
			free(pop->buffer);
			free(pop);
			return;
		}
		pre_pop = pre_pop->next;
		pop = pre_pop->next;
	}
}

void	announce(client* clients, fd_set* write_set, int index, char* announce)
{
	char buffer[128];
	*buffer = 0;

	sprintf(buffer, "server: client %d just %s\n", index, announce);

	size_t buffer_len = strlen(buffer);
	while (clients != NULL) {
		if (FD_ISSET(clients->fd, write_set))
			write(clients->fd, buffer, buffer_len);
		clients = clients->next;
	}
}

void	broadcast_message(server* server, int client_index, char* message, fd_set* write_set)
{
	char buffer[65000];
	*buffer = 0;

	sprintf(buffer, "client %d: %s", client_index, message);
	size_t buffer_len = strlen(buffer);

	client* clients = server->clients;
	while (clients != NULL) {
		if (clients->index != client_index && FD_ISSET(clients->fd, write_set))
			write(clients->fd, buffer, buffer_len);
		clients = clients->next;
	}
}

void	read_client(server* server, client* client, fd_set* read_set, fd_set* write_set)
{
	char buffer[2049];

	int read_returnval = recv(client->fd, buffer, 2048, 0);
	if (read_returnval < 0)
		exit_err("Fatal error\n");
	buffer[read_returnval] = '\0';

	if (read_returnval == 0) { // left message
		announce(server->clients, write_set, client->index, "left");
		pop_client(&(server->clients), client->fd);
		return;
	}

	client->buffer = str_join(client->buffer, buffer);
	
	// message
	char* message;
	char* newbuffer = client->buffer;
	int extract_returnval = extract_message(&newbuffer, &message);
	while (extract_returnval == 1) {
		broadcast_message(server, client->index, message, write_set);
		free(client->buffer);
		client->buffer = newbuffer;

		newbuffer = client->buffer;
		extract_returnval = extract_message(&newbuffer, &message);
	}
	if (extract_returnval == -1)
		exit_err("Fatal error\n");
}

void	read_fds(server* server, fd_set* read_set, fd_set* write_set)
{
	if (FD_ISSET(server->fd, read_set)) { // new client
		client* new_client = malloc(sizeof(*new_client));
		if (new_client == NULL)
			exit_err("Fatal error\n");

		int client_fd = accept(server->fd, NULL, NULL);
		if (client_fd == -1)
			exit_err("Fatal error\n");

		new_client->fd = client_fd;
		push_client(&(server->clients), new_client, server->next_index);
		server->next_index++;

		announce(server->clients, write_set, new_client->index, "arrived");
	}

	client* clients = server->clients;
	while (clients != NULL) {
		if (FD_ISSET(clients->fd, read_set)) { // new message
			client* next = clients->next;
			read_client(server, clients, read_set, write_set);
			clients = next;
		}
		else
			clients = clients->next;
	}
}

void	mini_serv(server* server)
{
	while (1) {
		fd_set read, write;
		build_fd_sets(server, &read, &write);

		struct timeval timeout = {0, 0};
		int return_val = select(FD_SETSIZE, &read, &write, NULL, &timeout);
		if (return_val == -1)
			exit_err("Fatal error\n");
		else if (return_val > 0)
			read_fds(server, &read, &write);
	}
}

int	main(int argc, char** argv)
{
	if (argc == 1)
		exit_err("Wrong number of arguments\n");

	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == -1)
		exit_err("Fatal error\n");

	struct sockaddr_in server_address;
	bzero(&server_address, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	server_address.sin_port = htons(atoi(argv[1]));

	if (bind(socket_fd, (const struct sockaddr*)&server_address, sizeof(server_address)) == -1)
		exit_err("Fatal error\n");

	if (listen(socket_fd, 10) == -1)
		exit_err("Fatal error\n");

	server	server = {socket_fd, NULL, 0};
	mini_serv(&server);

	return EXIT_SUCCESS;
}
//--
