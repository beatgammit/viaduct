#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#ifdef DEBUG
#include <stdio.h>
#endif

#ifdef USE_MSGPACK
#include "vendor/cmp.h"
#endif

#include "viaduct.h"

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
#ifdef DEBUG
		printf("Unexpected first byte\n");
#endif
		return 1;
	}

	if ((buf[1] & 0xf) != serialization) {
#ifdef DEBUG
		printf("serialization not agreed upon\n");
#endif
		return 1;
	}

	if ((buf[1] >> 4) != length) {
#ifdef DEBUG
		printf("length not negotiated\n");
#endif
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

#ifdef DEBUG
	//printf("Read: %d, %d, %d\n", n, cl->buf_len, cl->exp_len);
#endif

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

#ifdef USE_MSGPACK
void publish_msgpack(struct client* cl, const char* message, size_t len) {
	cl->i = 0;
	cl->buf_len = 0;

	cmp_ctx_t cmp;
	cmp_init(&cmp, cl, NULL, viaduct_msgpack_write_buf);

	cl->next_id++;

#ifdef DEBUG
	printf("Before\n");
#endif
	cmp_write_array(&cmp, 5);
	// message type
	cmp_write_uint(&cmp, WAMP_PUBLISH);
	// request id
	cmp_write_uint(&cmp, cl->next_id);
	// options
	cmp_write_map(&cmp, 0);
#ifdef DEBUG
	printf("After\n");
#endif
	// topic
	cmp_write_str(&cmp, "messages", 8);
	// arguments
	cmp_write_array(&cmp, 2);
	cmp_write_str(&cmp, message, len);
	cmp_write_str(&cmp, message, len);

	viaduct_send_message(cl, cl->buf, cl->buf_len);
}
#endif

void viaduct_publish_event(struct client* cl, const char* message, size_t msg_len) {
#ifdef DEBUG
	printf("Sending: [%d] %s\n", msg_len, message);
#endif
#ifdef USE_MSGPACK
	publish_msgpack(cl, message, msg_len);
#endif
}
