#define BUF_SIZE 256
#define MAX_LENGTH 0xf
#define RAW_SOCKET_JSON 1
#define RAW_SOCKET_MSGPACK 2

#define RAW_SOCKET_HEADER -1
#define RAW_SOCKET_MESSAGE 0
#define RAW_SOCKET_PING 1
#define RAW_SOCKET_PONG 2

#define MAGIC 0x7f

typedef struct client {
	int fd;
	unsigned char buf[BUF_SIZE];
	int buf_len;
	int exp_len;
	int msg_type;

	int (* read)(struct client*, unsigned char*, int);
	int (* write)(struct client*, const unsigned char*, int);
} client;

int lane_handshake(client* cl, int length, int serialization);
int lane_handle_message(struct client* cl);
int lane_bytes_to_len(unsigned char* buf, int len);
void lane_len_to_bytes(int len, unsigned char* buf);
