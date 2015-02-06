#include <stdint.h>

#define BUF_SIZE 512
#define MAX_LENGTH 0xf
#define RAW_SOCKET_JSON 1
#define RAW_SOCKET_MSGPACK 2

#define RAW_SOCKET_HEADER 0xff
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

struct client {
	void* data;

	uint8_t buf[BUF_SIZE];
	size_t buf_len;
	size_t exp_len;
	uint8_t msg_type;
	size_t i;
	uint64_t next_id;

	size_t (* read)(struct client*, uint8_t*, size_t);
	size_t (* write)(struct client*, const uint8_t*, size_t);

	void (* on_message)(const uint8_t*, size_t);
};

struct wamp_role {
	char* role;
	size_t len;
	// TODO: advanced features per role
};

struct wamp_welcome_details {
	struct wamp_role* roles;
	size_t len;
};

int viaduct_handshake(struct client* cl, uint8_t length, uint8_t serialization);
bool viaduct_handle_message(struct client* cl);
void viaduct_send_hello(struct client* cl, const char* realm, size_t realm_len, struct wamp_welcome_details* details);
void viaduct_publish_event(struct client* cl, const char* message, size_t msg_len);
