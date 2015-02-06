#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <errno.h>
#include <string.h>
#include <time.h>

#define SEND_EVERY_SEC 5

#include "viaduct.h"

struct socket_data {
	int fd;
};

size_t os_read(struct client* this, uint8_t* buf, size_t len) {
	struct socket_data* data = this->data;
	return read(data->fd, buf, len);
}
size_t os_write(struct client* this, const uint8_t* buf, size_t len) {
	struct socket_data* data = this->data;
	return write(data->fd, buf, len);
}

void print_errno(int err) {
	switch (err) {
	case EACCES:
		printf("EACCES (%d): For UNIX domain sockets, which are identified by pathname: Write permission is denied on the socket file, or search permission is denied for one of the directories in the path prefix. (See also path_resolution(7).)\n", err);
		break;
	case EPERM:
		// EACCES, EPERM
		printf("EPERM (%d): The user tried to connect to a broadcast address without having the socket broadcast flag enabled or the connection request failed because of a local firewall rule.\n", err);
		break;
	case EADDRINUSE:
		printf("EADDRINUSE (%d): Local address is already in use.\n", err);
		break;
	case EAFNOSUPPORT:
		printf("EAFNOSUPPORT (%d): The passed address didn't have the correct address family in its sa_family field.\n", err);
		break;
	case EAGAIN:
		printf("EAGAIN (%d): No more free local ports or insufficient entries in the routing cache. For AF_INET see the description of /proc/sys/net/ipv4/ip_local_port_range ip(7) for information on how to increase the number of local ports.\n", err);
		break;
	case EALREADY:
		printf("EALREADY (%d): The socket is nonblocking and a previous connection attempt has not yet been completed.\n", err);
		break;
	case EBADF:
		printf("EBADF (%d): The file descriptor is not a valid index in the descriptor table.\n", err);
		break;
	case ECONNREFUSED:
		printf("ECONNREFUSED (%d): No-one listening on the remote address.\n", err);
		break;
	case EFAULT:
		printf("EFAULT (%d): The socket structure address is outside the user's address space.\n", err);
		break;
	case EINPROGRESS:
		printf("EINPROGRESS (%d): The socket is nonblocking and the connection cannot be completed immediately. It is possible to select(2) or poll(2) for completion by selecting the socket for writing. After select(2) indicates writability, use getsockopt(2) to read the SO_ERROR option at level SOL_SOCKET to determine whether connect() completed successfully (SO_ERROR is zero) or unsuccessfully (SO_ERROR is one of the usual error codes listed here, explaining the reason for the failure).\n", err);
		break;
	case EINTR:
		printf("EINTR (%d): The system call was interrupted by a signal that was caught; see signal(7).\n", err);
		break;
	case EISCONN:
		printf("EISCONN (%d): The socket is already connected.\n", err);
		break;
	case ENETUNREACH:
		printf("ENETUNREACH (%d): Network is unreachable.\n", err);
		break;
	case ENOTSOCK:
		printf("ENOTSOCK (%d): The file descriptor is not associated with a socket.\n", err);
		break;
	case ETIMEDOUT:
		printf("ETIMEDOUT (%d): Timeout while attempting connection. The server may be too busy to accept new connections. Note that for IP sockets the timeout may be very long when syncookies are enabled on the server.\n", err);
		break;
	}
}

int create_socket(char* addr, short port) {
	struct sockaddr_in sin;

	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketfd < 0) {
		printf("error creating socket\n");
		return -1;
	}

	memset(&sin, 0, sizeof(struct sockaddr_in));

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	if (inet_pton(AF_INET, addr, &sin.sin_addr) <= 0) {
		printf("inet_pton error\n");
		return -1;
	}

	if (connect(socketfd, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
		printf("error connecting: ");
		print_errno(errno);
		return -1;
	}

	return socketfd;
}

int main(int argc, char* argv[]) {
	struct client a;
	struct socket_data data;

	memset(&a, 0, sizeof(a));
	memset(&data, 0, sizeof(data));
	a.data = &data;

	data.fd = create_socket("127.0.0.1", 9000);
	if (data.fd < 0) {
		return 1;
	}
	a.read = os_read;
	a.write = os_write;

	if (viaduct_handshake(&a, MAX_LENGTH, RAW_SOCKET_MSGPACK)) {
		printf("failed handshake\n");
		return 1;
	}

	struct wamp_role role = {"publisher", 9};
	struct wamp_welcome_details details = {&role, 1};
	viaduct_send_hello(&a, "turnpike.example", 16, &details);

	a.buf_len = 0;
	a.exp_len = 4;

	viaduct_handle_message(&a);

	// set socket as non-blocking
	int flags = fcntl(data.fd, F_GETFL, 0);
	if (flags < 0) {
		printf("error getting flags for fd\n");
		return 1;
	}
	if (fcntl(data.fd, F_SETFL, flags | O_NONBLOCK) < 0) {
		printf("error setting socket to non-blocking mode\n");
		return 1;
	}

	a.msg_type = RAW_SOCKET_HEADER;
	a.buf_len = 0;
	a.exp_len = 4;

	time_t last_sent = time(NULL);
	time_t last_recv = 0;
	for (;;) {
		time_t now = time(NULL);
		if (viaduct_handle_message(&a)) {
			last_recv = now;
		}
		double diff = difftime(now, last_sent);
		if (diff >= SEND_EVERY_SEC) {
			printf("Time to send message\n");

			wamp_type_list args;
			struct wamp_type arg_list[] = {
				{
					.type = TYPE_STRING,
					.string = {.len = 12, .val = "some message"},
				},
				{
					.type = TYPE_STRING,
					.string = {.len = 12, .val = "some message"},
				},
			};
			args.len = 2;
			args.val = arg_list;

			wamp_type_string topic = {.len = 8, .val ="messages"};
			viaduct_publish(&a, topic, &args, NULL);
			last_sent = now;
		}
		if (difftime(now, last_recv) >= 1 && SEND_EVERY_SEC - diff > 1) {
			sleep(1);
		}
	}

	return 0;
}
