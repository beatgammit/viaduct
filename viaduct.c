#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#ifdef DEBUG
#include <stdarg.h>
#include <stdio.h>
#endif

#ifdef USE_MSGPACK
#include "vendor/cmp.h"
#endif

#include "viaduct.h"

void debug(char* fmt, ...) {
#ifdef DEBUG
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
#endif
}

void viaduct_send_message(struct client* cl, uint8_t* buf, size_t len);

// viaduct_handshake handles raw socket handshaking
// returns 0 on success or a non-zero error code on failure
int viaduct_handshake(struct client* cl, uint8_t length, uint8_t serialization) {
	uint8_t buf[4] = {MAGIC, (length << 4) | serialization, 0, 0};

	size_t n = cl->write(cl, buf, 4);
	if (n != 4) {
		return 1;
	}

	n = cl->read(cl, buf, 4);
	if (n != 4) {
		return 1;
	}

	if (buf[0] != MAGIC) {
		debug("Unexpected first byte\n");
		return 1;
	}

	if ((buf[1] & 0xf) != serialization) {
		debug("serialization not agreed upon\n");
		return 1;
	}

	if ((buf[1] >> 4) != length) {
		debug("length not negotiated\n");
	}

	cl->msg_type = RAW_SOCKET_HEADER;

	return 0;
}

void viaduct_len_to_bytes(uint32_t len, uint8_t* buf) {
	buf[0] = (uint8_t)(len >> 16);
	buf[1] = (uint8_t)(len >> 8);
	buf[2] = (uint8_t)(len);
}

uint32_t viaduct_bytes_to_len(uint8_t* buf) {
	return ((uint32_t)(buf[0]) << 16) | ((uint32_t)(buf[1]) << 8) | buf[2];
}

#ifdef USE_MSGPACK
bool viaduct_msgpack_read_buf(struct cmp_ctx_s *ctx, void *data, size_t limit) {
	struct client* cl = (struct client*)ctx->buf;
	memcpy(data, cl->buf, limit);
	cl->i += limit;
	return true;
}

size_t viaduct_msgpack_write_buf(struct cmp_ctx_s *ctx, const void *data, size_t limit) {
	struct client* cl = (struct client*)ctx->buf;
	memcpy(cl->buf + cl->buf_len, data, limit);
	cl->buf_len += limit;
	return limit;
}

void hello_msgpack(struct client* cl, const char* realm, size_t realm_len, struct wamp_welcome_details* details) {
	cl->i = 0;

	cmp_ctx_t cmp;
	cmp_init(&cmp, cl, NULL, viaduct_msgpack_write_buf);

	cmp_write_array(&cmp, 3);
	cmp_write_uint(&cmp, WAMP_HELLO);
	cmp_write_str(&cmp, realm, realm_len);

	// write details
	cmp_write_map(&cmp, 1);
	cmp_write_str(&cmp, "roles", 5);

	cmp_write_map(&cmp, details->len);
	for (size_t i = 0; i < details->len; i++) {
		cmp_write_str(&cmp, details->roles[i].role, details->roles[i].len);
		cmp_write_map(&cmp, 0);
	}
}
#endif

void viaduct_send_hello(struct client* cl, const char* realm, size_t realm_len, struct wamp_welcome_details* details) {
#ifdef USE_MSGPACK
	hello_msgpack(cl, realm, realm_len, details);
#endif

	viaduct_send_message(cl, cl->buf, cl->buf_len);
}

bool viaduct_handle_message(struct client* cl) {
	size_t n = cl->read(cl, cl->buf+cl->buf_len, cl->exp_len);
	cl->buf_len += n;
	cl->exp_len -= n;

	//debug("Read: %d, %d, %d\n", n, cl->buf_len, cl->exp_len);

	if (cl->exp_len > 0) {
		return false;
	}

	// we have enough data
	switch (cl->msg_type) {
	case RAW_SOCKET_MESSAGE:
		// TODO: parse message
		break;
	case RAW_SOCKET_PING:
		cl->write(cl, cl->buf, cl->buf_len);
		break;
	case RAW_SOCKET_PONG:
		break;
	case RAW_SOCKET_HEADER:
		// header processed
		// TODO: check for invalid message type
		cl->msg_type = cl->buf[0];

		cl->buf_len = 0;
		cl->exp_len = viaduct_bytes_to_len(cl->buf+1);
		if (cl->exp_len > MAX_LENGTH) {
			// TODO: handle too-long messages
		}
		cl->buf[cl->exp_len] = 0;
		return viaduct_handle_message(cl);
	}
	cl->msg_type = RAW_SOCKET_HEADER;
	return true;
}

void viaduct_write_header(struct client* cl, size_t len) {
	uint8_t header[4];
	header[0] = 0;
	viaduct_len_to_bytes(len, header+1);
	cl->write(cl, header, 4);
}

void viaduct_send_message(struct client* cl, uint8_t* buf, size_t len) {
	viaduct_write_header(cl, len);
	cl->write(cl, buf, len);
}

uint64_t viaduct_next_request_id(struct client* cl) {
	cl->next_id++;
	return cl->next_id;
}

#ifdef USE_MSGPACK
void msgpack_write_type(cmp_ctx_t* ctx, struct wamp_type type);

void msgpack_write_list(cmp_ctx_t* ctx, wamp_type_list list) {
	cmp_write_array(ctx, list.len);
	for (int i = 0; i < list.len; i++) {
		msgpack_write_type(ctx, list.val[i]);
	}
}

void msgpack_write_dict(cmp_ctx_t* ctx, wamp_type_dict dict) {
	cmp_write_array(ctx, dict.len);
	for (int i = 0; i < dict.len; i++) {
		struct wamp_key_val entry = dict.entries[i];
		cmp_write_str(ctx, entry.key, entry.key_len);
		msgpack_write_type(ctx, *entry.val);
	}
}

void msgpack_write_type(cmp_ctx_t* ctx, struct wamp_type type) {
	switch (type.type) {
	case TYPE_INT:
		cmp_write_sint(ctx, type.integer);
		break;
	case TYPE_BOOL:
		cmp_write_bool(ctx, type.boolean);
		break;
	case TYPE_STRING:
		cmp_write_str(ctx, type.string.val, type.string.len);
		break;
	case TYPE_LIST:
		msgpack_write_list(ctx, type.list);
		break;
	case TYPE_DICT:
		msgpack_write_dict(ctx, type.dict);
		break;
	case TYPE_FLOAT:
		cmp_write_double(ctx, type.number);
		break;
	}
}

void publish_msgpack(struct client* cl, const wamp_type_string topic, const wamp_type_list* args, const wamp_type_dict* kw_args) {
	cl->i = 0;
	cl->buf_len = 0;

	cmp_ctx_t cmp;
	cmp_init(&cmp, cl, NULL, viaduct_msgpack_write_buf);

	cl->next_id++;

	int msg_len = 4;
	if (kw_args != NULL && kw_args->len > 0) {
		msg_len = 6;
	} else if (args != NULL && args->len > 0) {
		msg_len = 5;
	}
	debug("msg_len: %d\n", msg_len);
	cmp_write_array(&cmp, msg_len);

	// message type
	cmp_write_uint(&cmp, WAMP_PUBLISH);
	// request id
	cmp_write_uint(&cmp, viaduct_next_request_id(cl));
	// options
	cmp_write_map(&cmp, 0);
	debug("After\n");
	// topic
	cmp_write_str(&cmp, topic.val, topic.len);
	if (msg_len > 4) {
		if (args == NULL) {
			cmp_write_array(&cmp, 0);
		} else {
			debug("args: %d\n", args->len);
			msgpack_write_list(&cmp, *args);
		}
	}

	if (msg_len > 5) {
		debug("kw_args: %d\n", kw_args->len);
		msgpack_write_dict(&cmp, *kw_args);
	}
	viaduct_send_message(cl, cl->buf, cl->buf_len);
}
#endif

void viaduct_publish(struct client* cl, const wamp_type_string topic, const wamp_type_list* args, const wamp_type_dict* kw_args) {
	//debug("Sending: [%d] %s\n", msg_len, message);
#ifdef USE_MSGPACK
	publish_msgpack(cl, topic, args, kw_args);
#endif
}
