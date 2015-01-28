#include <stdbool.h>
#include <unistd.h>

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
		// printf("Unexpected first byte\n");
		return 1;
	}

	if ((buf[1] & 0xf) != serialization) {
		// printf("serialization not agreed upon\n");
		return 1;
	}

	if ((buf[1] >> 4) != length) {
		// printf("length not negotiated\n");
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

int lane_handle_message(struct client* cl) {
	int n = cl->read(cl, cl->buf+cl->buf_len, cl->exp_len);
	cl->buf_len += n;
	cl->exp_len -= n;

	if (cl->exp_len > 0) {
		return 0;
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
		cl->buf[cl->exp_len] = 0;
		break;
	}
	cl->msg_type = RAW_SOCKET_HEADER;
	return 0;
}
