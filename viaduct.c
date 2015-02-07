#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#ifdef DEBUG
#include <stdarg.h>
#include <stdio.h>
#endif

#include "vendor/cmp.h"

#include "viaduct.h"

void debug(char* fmt, ...) {
#ifdef DEBUG
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
#endif
}

void viaduct_send_message(struct wamp_client* cl, uint8_t* buf, size_t len);

struct wamp_type viaduct_empty_dict() {
	struct wamp_type t = { TYPE_DICT };
	t.dict.len = 0;
	return t;
}

// viaduct_handshake handles raw socket handshaking
// returns 0 on success or a non-zero error code on failure
int viaduct_handshake(struct wamp_client* cl, struct raw_socket_options opts) {
	uint8_t buf[4] = {MAGIC, (opts.length << 4) | opts.serialization, 0, 0};

	size_t n = cl->write(cl, buf, 4);
	if (n != 4) {
		return 1;
	}

	cl->exp_len = 4;
	cl->buf_len = 0;

	n = cl->read(cl, buf, 4);
	if (n != 4) {
		return 1;
	}

	if (buf[0] != MAGIC) {
		debug("Unexpected first byte\n");
		return 1;
	}

	if ((buf[1] & 0xf) != opts.serialization) {
		debug("serialization not agreed upon\n");
		return 1;
	}

	if ((buf[1] >> 4) != opts.length) {
		debug("length not negotiated\n");
	}

	cl->msg_type = RAW_SOCKET_HEADER;
	cl->buf_len = 0;
	cl->exp_len = 4;
	cl->serialize = opts.serialize;

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

uint8_t num_bits_set(uint8_t val) {
	uint8_t ret = 0;
	while (val > 0) {
		if (val & 1) {
			ret++;
		}
		val >>= 1;
	}
	return ret;
}

void viaduct_join_realm(struct wamp_client* cl, const char* realm, size_t realm_len, wamp_type_dict details) {
	struct wamp_type msg[3] = {
		{ TYPE_INT, },
		{ TYPE_STRING },
		{ TYPE_DICT },
	};
	msg[0].integer = WAMP_HELLO;
	msg[1].string.len = realm_len;
	msg[1].string.val = realm;
	wamp_type_list msglist = { 3, msg };
	cl->serialize(cl, msglist);
	viaduct_send_message(cl, cl->buf, cl->buf_len);
}

bool viaduct_handle_message(struct wamp_client* cl) {
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
	cl->buf_len = 0;
	cl->exp_len = 4;
	return true;
}

void viaduct_write_header(struct wamp_client* cl, size_t len) {
	uint8_t header[4];
	header[0] = 0;
	viaduct_len_to_bytes(len, header+1);
	cl->write(cl, header, 4);
}

void viaduct_send_message(struct wamp_client* cl, uint8_t* buf, size_t len) {
	viaduct_write_header(cl, len);
	cl->write(cl, buf, len);
}

uint64_t viaduct_next_request_id(struct wamp_client* cl) {
	cl->next_id++;
	return cl->next_id;
}

void msgpack_write_type(cmp_ctx_t* ctx, struct wamp_type type);

void msgpack_write_list(cmp_ctx_t* ctx, wamp_type_list list) {
	cmp_write_array(ctx, list.len);
	int i;
	for (i = 0; i < list.len; i++) {
		msgpack_write_type(ctx, list.val[i]);
	}
}

void msgpack_write_dict(cmp_ctx_t* ctx, wamp_type_dict dict) {
	cmp_write_map(ctx, dict.len);
	int i;
	for (i = 0; i < dict.len; i++) {
		struct wamp_key_val entry = dict.entries[i];
		cmp_write_str(ctx, entry.key, entry.key_len);
		msgpack_write_type(ctx, entry.val);
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

bool viaduct_msgpack_read_buf(struct cmp_ctx_s *ctx, void *data, size_t limit) {
	struct wamp_client* cl = (struct wamp_client*)(ctx->buf);
	memcpy(data, cl->buf, limit);
	cl->i += limit;
	return true;
}

size_t viaduct_msgpack_write_buf(struct cmp_ctx_s *ctx, const void *data, size_t limit) {
	struct wamp_client* cl = (struct wamp_client*)(ctx->buf);
	memcpy(cl->buf + cl->buf_len, data, limit);
	cl->buf_len += limit;
	return limit;
}

void serialize_msgpack(struct wamp_client* cl, const wamp_type_list msg) {
	cmp_ctx_t cmp;
	cmp_init(&cmp, cl, NULL, viaduct_msgpack_write_buf);

	cl->i = 0;
	cl->buf_len = 0;

	msgpack_write_list(&cmp, msg);
	viaduct_send_message(cl, cl->buf, cl->buf_len);
}

void viaduct_publish(struct wamp_client* cl, const wamp_type_dict* options, const wamp_type_string topic, const wamp_type_list* args, const wamp_type_dict* kw_args) {
	int msg_len = 4;

	struct wamp_type msg[6] = {
		{ TYPE_INT },
		{ TYPE_INT },
		{ TYPE_DICT },
		{ TYPE_STRING },
		{ TYPE_LIST },
		{ TYPE_DICT },
	};
	msg[0].integer = WAMP_PUBLISH;
	msg[1].integer = viaduct_next_request_id(cl);
	msg[3].string = topic;
	if (options == NULL) {
		msg[2].dict.len = 0;
	} else {
		msg[2].dict = *options;
	}

	if (kw_args != NULL && kw_args->len > 0) {
		if (args == NULL ) {
			msg[4].list.len = 0;
		} else {
			msg[4].list = *args;
		}
		msg[5].dict = *kw_args;
	} else if (args != NULL && args->len > 0) {
		msg_len = 5;
		msg[4].list = *args;
	}

	wamp_type_list msglist = { msg_len, msg };
	cl->serialize(cl, msglist);
}

