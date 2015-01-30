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

#include "lane.h"
#include "vendor/cmp.h"

int os_read(struct client* this, unsigned char* buf, int len) {
	return read(this->fd, buf, len);
}
int os_write(struct client* this, const unsigned char* buf, int len) {
	return write(this->fd, buf, len);
}

bool lane_msgpack_read_buf(struct cmp_ctx_s *ctx, void *data, size_t limit) {
	struct client* cl = (struct client*)ctx->buf;
	memcpy(data, cl->buf, limit);
	cl->i += limit;
	return true;
}

size_t lane_msgpack_write_buf(struct cmp_ctx_s *ctx, const void *data, size_t limit) {
	struct client* cl = (struct client*)ctx->buf;
	memcpy(cl->buf + cl->buf_len, data, limit);
	cl->buf_len += limit;
	return limit;
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

void hello_msgpack(client* cl, const char* realm, int realm_len, struct wamp_welcome_details* details) {
	cl->i = 0;

	cmp_ctx_t cmp;
	cmp_init(&cmp, cl, NULL, lane_msgpack_write_buf);

	cmp_write_array(&cmp, 3);
	cmp_write_uint(&cmp, WAMP_HELLO);
	cmp_write_str(&cmp, realm, realm_len);

	// write details
	cmp_write_map(&cmp, 1);
	cmp_write_str(&cmp, "roles", 5);

	cmp_write_map(&cmp, details->len);
	for (int i = 0; i < details->len; i++) {
		cmp_write_str(&cmp, details->roles[i].role, details->roles[i].len);
		cmp_write_map(&cmp, 0);
	}

	lane_send_message(cl, cl->buf, cl->buf_len);
}

void on_message(client* cl, unsigned char* buf, int buf_len) {
	cmp_ctx_t cmp;
	cmp_init(&cmp, cl, lane_msgpack_read_buf, NULL);

	int len;
	cmp_read_array(&cmp, &len);

	int msg_type;
	cmp_read_uint(&cmp, &msg_type);

	switch (msg_type) {
	case WAMP_WELCOME: {
		// TODO: store sess_id somewhere, if we care
		int sess_id;
		cmp_read_uint(&cmp, &sess_id);
		break;
	}
	case WAMP_EVENT: {
		printf("Received event\n");
		cmp_object_t obj;
		// TODO: don't care about sub id...
		cmp_read_object(&cmp, &obj);
		// TODO: don't care about pub id
		cmp_read_object(&cmp, &obj);

		int depth = 0;
		// skip through the details map for now
		do {
			if (!cmp_read_object(&cmp, &obj)) {
				printf("Handle error...\n");
			} else {
				switch (obj.type) {
					case CMP_TYPE_FIXMAP:
					case CMP_TYPE_MAP16:
					case CMP_TYPE_MAP32:
					case CMP_TYPE_FIXARRAY:
					case CMP_TYPE_ARRAY16:
					case CMP_TYPE_ARRAY32:
						depth++;
						break;
				}
			}
		} while (depth > 0);

		if (cmp_read_object(&cmp, &obj)) {
			if (obj.type == CMP_TYPE_FIXARRAY || obj.type == CMP_TYPE_ARRAY16 || obj.type == CMP_TYPE_ARRAY32) {
				for (int i = 0; i < obj.as.array_size; i++) {
					uint32_t size;
					char* buf;
					if (!cmp_read_str(&cmp, buf, &size)) {
						printf("Failed reading str\n");
					} else {
						printf("Arg[%d] -> %s\n", i, buf);
					}
				}
			}
		}
		break;
	 }
	default:
		// TODO: report warning that we don't support this message
		break;
	}
}

void send_event(client* cl, char* message, int len) {
	cl->i = 0;
	cl->buf_len = 0;

	cmp_ctx_t cmp;
	cmp_init(&cmp, cl, NULL, lane_msgpack_write_buf);

	cl->next_id++;

	printf("Before\n");
	cmp_write_array(&cmp, 5);
	cmp_write_uint(&cmp, WAMP_PUBLISH);
	cmp_write_uint(&cmp, cl->next_id);
	cmp_write_map(&cmp, 0);
	printf("After\n");
	cmp_write_str(&cmp, "messages", 8);
	cmp_write_array(&cmp, 2);
	cmp_write_str(&cmp, message, len);
	cmp_write_str(&cmp, message, len);

	lane_send_message(cl, cl->buf, cl->buf_len);
}

int main(int argc, char* argv[]) {
	client a;

	memset(&a, 0, sizeof(a));

	a.fd = create_socket("127.0.0.1", 9000);
	if (a.fd < 0) {
		return 1;
	}
	a.read = os_read;
	a.write = os_write;

	if (lane_handshake(&a, MAX_LENGTH, RAW_SOCKET_MSGPACK)) {
		printf("failed handshake\n");
		return 1;
	}

	wamp_role role = {"publisher", 9};
	wamp_welcome_details details = {&role, 1};
	hello_msgpack(&a, "turnpike.example", 16, &details);

	a.buf_len = 0;
	a.exp_len = 4;

	lane_handle_message(&a);

	// set socket as non-blocking
	int flags = fcntl(a.fd, F_GETFL, 0);
	if (flags < 0) {
		printf("error getting flags for fd\n");
		return 1;
	}
	if (fcntl(a.fd, F_SETFL, flags | O_NONBLOCK) < 0) {
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
		if (lane_handle_message(&a)) {
			last_recv = now;
		}
		double diff = difftime(now, last_sent);
		if (diff >= SEND_EVERY_SEC) {
			printf("Time to send message\n");
			send_event(&a, "some message", 12);
			last_sent = now;
		}
		if (difftime(now, last_recv) >= 1 && SEND_EVERY_SEC - diff > 1) {
			sleep(1);
		}
	}

	return 0;
}
