SRCS = $(wildcard *.c)

PROGS = $(patsubst %.c,%,$(SRCS))

all: $(PROGS)

%: %.c
	$(CC) -ltap -llane -L.. -I.. $(CFLAGS) -o $@ $<
	./$@

clean:
	rm -f main
