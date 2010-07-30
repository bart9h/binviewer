#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <curses.h>

typedef struct {
	unsigned char* buf;
	off_t size;
	int page_cursor;
	int cursor;
	int width, height;
	int bytes_per_line;
	int chars_per_byte;
} binviewer_state_t;

void display (binviewer_state_t* st)
{
	getmaxyx(stdscr, st->height, st->width);

	st->bytes_per_line = 1;
	st->chars_per_byte = 3;  // "FF "
	while (st->bytes_per_line*2 < (st->width+1)/(st->chars_per_byte))
		st->bytes_per_line *= 2;

	unsigned char* p = st->buf + st->page_cursor;
	for (int j = 0; j < st->height; ++j)
	for (int i = 0; i < st->bytes_per_line; ++i) {
		if (st->page_cursor + i+j*st->bytes_per_line >= st->size)
			return;
		mvprintw(j, i*st->chars_per_byte, "%02x ", *p);
		++p;
	}
}

void binviewer (void* buf, off_t size)
{
	binviewer_state_t state, *st = &state;
	st->buf = (unsigned char*) buf;
	st->size = size;
	st->page_cursor = 0;
	st->cursor = 0;

	initscr();
	keypad(stdscr, TRUE);

	int running = 1;
	while(running) {
		display(st);
		refresh();
		switch (mvgetch(0,0)) {
		case 'q':
			running = 0;
			break;
		}
	}

	endwin();
}

int main (int argc, char* argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		return 1;
	}
	const char* filename = argv[1];

	int fd = open(filename, O_RDONLY);
	if (fd < 1) {
		perror(filename);
		return 2;
	}

	struct stat st;
	assert (fstat(fd, &st) == 0);

	void* buf = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	assert(buf != MAP_FAILED);

	binviewer(buf, st.st_size);

	return 0;
}

