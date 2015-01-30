#define BUF_SIZE 256
#define MAX_LENGTH 0xf
#define RAW_SOCKET_JSON 1
#define RAW_SOCKET_MSGPACK 2

#define RAW_SOCKET_HEADER -1
#define RAW_SOCKET_MESSAGE 0
#define RAW_SOCKET_PING 1
#define RAW_SOCKET_PONG 2

#define WAMP_HELLO 1
#define WAMP_WELCOME 2
#define WAMP_ABORT 3
#define WAMP_CHALLENGE 4
#define WAMP_AUTHENTICATE 5
#define WAMP_GOODBYE 6
#define WAMP_HEARTBEAT 7
#define WAMP_ERROR 8

#define WAMP_PUBLISH 16
#define WAMP_PUBLISHED 17

#define WAMP_SUBSCRIBE 32
#define WAMP_SUBSCRIBED 33
#define WAMP_UNSUBSCRIBE 34
#define WAMP_UNSUBSCRIBED 35
#define WAMP_EVENT 36

#define WAMP_CALL 48
#define WAMP_CANCEL 49
#define WAMP_RESULT 50

#define WAMP_REGISTER 64
#define WAMP_REGISTERED 65
#define WAMP_UNREGISTER 66
#define WAMP_UNREGISTERED 67
#define WAMP_INVOCATION 68
#define WAMP_INTERRUPT 69
#define WAMP_YIELD 70

#define MAGIC 0x7f

typedef struct client {
	int fd;
	unsigned char buf[BUF_SIZE];
	int buf_len;
	int exp_len;
	int msg_type;
	int i;
	int next_id;

	int (* read)(struct client*, unsigned char*, int);
	int (* write)(struct client*, const unsigned char*, int);

	void (* on_message)(const unsigned char*, int);
} client;

typedef struct wamp_role {
	char* role;
	int len;
	// TODO: advanced features per role
} wamp_role;

typedef struct wamp_welcome_details {
	wamp_role* roles;
	int len;
} wamp_welcome_details;

int lane_handshake(client* cl, int length, int serialization);
bool lane_handle_message(struct client* cl);
int lane_bytes_to_len(unsigned char* buf, int len);
void lane_len_to_bytes(int len, unsigned char* buf);
void lane_send_message(client* cl, unsigned char* buf, int len);
