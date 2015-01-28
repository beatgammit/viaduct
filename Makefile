CFLAGS = -g -Wall -I. -fPIC -Ivendor/ -std=c99
LDFLAGS += -ltap
TEST_SRC = $(wildcard test/*.c)
TEST_PROGS = $(patsubst %.c,%,$(TEST_SRC))

%: %.c
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $(filter %.o %.a %.so, $^) $(LDLIBS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(filter %.c, $^) $(LDLIBS) -o $@

%.a:
	$(AR) rcs $@ $(filter %.o, $^)

%.so:
	$(CC) -shared $(LDFLAGS) $(TARGET_ARCH) $(filter %.o, $^) $(LDLIBS) -o $@

all: liblane.a liblane.so example # test

cmp.o: vendor/cmp.c vendor/cmp.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(filter %.c, $^) $(LDLIBS) -o $@

liblane.a: lane.o

liblane.so: lane.o

lane.o: cmp.o lane.c lane.h

# test: $(TEST_PROGS)
# 	$(foreach test,$(TEST_PROGS),$(test);)

$(TESTS): %: %.o liblane.so

example: liblane.so main.c cmp.o
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $(LDLIBS) liblane.so cmp.o main.c -o example

clean:
	rm -rf *.o test/*.o *.a *.so example $(TESTS) $(TEST_PROGS)

.PHONY: all clean test
