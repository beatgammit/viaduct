#include <stdbool.h>
#include <unistd.h>

#define DEBUG

#ifdef DEBUG
#include <stdio.h>
#endif

#include "lane.h"

// lane_handshake handles raw socket handshaking
// returns 0 on success or a non-zero error code on failure
int lane_handshake(client* cl, int length, int serialization) {
	unsigned char buf[4];
	buf[0] = MAGIC;
	buf[1] = (length << 4) | serialization;
	buf[2] = 0;
	buf[3] = 0;

	int n = cl->write(cl, buf, 4);
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

void lane_len_to_bytes(int len, unsigned char* buf) {
	buf[0] = (unsigned char)(len >> 16);
	buf[1] = (unsigned char)(len >> 8);
	buf[2] = (unsigned char)(len);
}

int lane_bytes_to_len(unsigned char* buf, int len) {
	return ((int)(buf[0]) << 16) | ((int)(buf[1]) << 8) | buf[2];
}

bool lane_handle_message(struct client* cl) {
	int n = cl->read(cl, cl->buf+cl->buf_len, cl->exp_len);
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
		cl->msg_type = (int)cl->buf[0];

		cl->buf_len = 0;
		cl->exp_len = lane_bytes_to_len(cl->buf+1, 3);
		if (cl->exp_len > MAX_LENGTH) {
			// TODO: handle too-long messages
		}
		cl->buf[cl->exp_len] = 0;
		return lane_handle_message(cl);
	}
	cl->msg_type = RAW_SOCKET_HEADER;
	return true;
}

void lane_write_header(client* cl, int len) {
	unsigned char header[4];
	header[0] = 0;
	lane_len_to_bytes(len, header+1);
	cl->write(cl, header, 4);
}

void lane_send_message(client* cl, unsigned char* buf, int len) {
	lane_write_header(cl, len);
	cl->write(cl, buf, len);
}
