#include <stdint.h>

#define MAX_LENGTH 0
#define BUF_SIZE (2 << (9 + MAX_LENGTH))
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

#define TYPE_INT (1 << 0)
#define TYPE_BOOL (1 << 1)
#define TYPE_STRING (1 << 2)
#define TYPE_LIST (1 << 3)
#define TYPE_DICT (1 << 4)
#define TYPE_FLOAT (1 << 5)

struct wamp_type;

struct wamp_key_val {
	size_t key_len;
	char* key;
	struct wamp_type* val;
};

typedef int64_t wamp_type_int;

typedef bool wamp_type_bool;

typedef struct {
	size_t len;
	char* val;
} wamp_type_string;

typedef struct {
	size_t len;
	struct wamp_type* val;
} wamp_type_list;

typedef struct {
	size_t len;
	struct wamp_key_val* entries;
} wamp_type_dict;

typedef double wamp_type_float;

struct wamp_type {
	int8_t type;

	union {
		wamp_type_int integer;
		wamp_type_bool boolean;
		wamp_type_string string;
		wamp_type_list list;
		wamp_type_dict dict;
		wamp_type_float number;
	};
};

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
void viaduct_publish(struct client* cl, const wamp_type_string topic, const wamp_type_list* args, const wamp_type_dict* kw_args);
