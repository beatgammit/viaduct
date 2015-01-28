#include <stdbool.h>
#include <string.h>

#include <tap.h>
#include <lane.h>

#define BYTES_TO_LEN_TESTS 3
#define LEN_TO_BYTES_TESTS 3
#define TESTS (BYTES_TO_LEN_TESTS + LEN_TO_BYTES_TESTS)

void test_bytes_to_len() {
	unsigned char bytes[3] = {0, 0, 3};
	int val = lane_bytes_to_len(bytes, 3);

	cmp_ok(val, "==", 3, "bytes to len, small value");

	bytes[1] = 3;
	bytes[2] = 0;

	val = lane_bytes_to_len(bytes, 3);
	cmp_ok(val, "==", 3 << 8, "bytes to len, medium value");

	bytes[0] = 3;
	bytes[1] = 0;

	val = lane_bytes_to_len(bytes, 3);
	cmp_ok(val, "==", 3 << 16, "bytes to len, large value");
}

void test_len_to_bytes() {
	unsigned char bytes[3];

	memset(bytes, 5, sizeof(bytes));
	lane_len_to_bytes(3, bytes);
	ok(bytes[0] == 0 && bytes[1] == 0 && bytes[2] == 3, "len to bytes, small value");

	memset(bytes, 5, sizeof(bytes));
	lane_len_to_bytes(3 << 8, bytes);
	ok(bytes[0] == 0 && bytes[1] == 3 && bytes[2] == 0, "len to bytes, medium value");

	memset(bytes, 5, sizeof(bytes));
	lane_len_to_bytes(3 << 16, bytes);
	ok(bytes[0] == 3 && bytes[1] == 0 && bytes[2] == 0, "len to bytes, large value");
}

int main(int argc, char* argv[]) {
	plan(TESTS);

	test_bytes_to_len();
	test_len_to_bytes();

	done_testing();
}
