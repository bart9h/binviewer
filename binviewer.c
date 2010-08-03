#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <curses.h>

typedef struct {
	bool running;
	bool do_swap;
	unsigned char* buf;
	off_t size;
	int page_cursor;
	int cursor;
	int width, height;
	int bytes_per_line;
	int chars_per_byte;
	int header_lines;
	int attr;
} binviewer_state_t;

void swap2 (void* ptr)
{
	char* p = (char*) ptr;
	char tmp = p[0];
	p[0] = p[1];
	p[1] = tmp;
}

void swap4 (void* ptr)
{
	uint32_t t = *((uint32_t*)ptr);
	char* a = (char*) ptr;
	char* b = (char*) &t;
	a[0] = b[3];
	a[1] = b[2];
	a[2] = b[1];
	a[3] = b[0];
}

void swap8 (void* ptr)
{
	uint64_t t = *((uint64_t*)ptr);
	char* a = (char*) ptr;
	char* b = (char*) &t;
	a[0] = b[7];
	a[1] = b[6];
	a[2] = b[5];
	a[3] = b[4];
	a[4] = b[3];
	a[5] = b[2];
	a[6] = b[1];
	a[7] = b[0];
}

void display_data (binviewer_state_t* st)
{
	unsigned char* p = st->buf + st->cursor;

	int8_t   i8  = *((int8_t  *) p);
	uint8_t  u8  = *((uint8_t *) p);
	int16_t  i16 = *((int16_t *) p);
	uint16_t u16 = *((uint16_t*) p);
	int32_t  i32 = *((int32_t *) p);
	uint32_t u32 = *((uint32_t*) p);
	int64_t  i64 = *((int64_t *) p);
	uint64_t u64 = *((uint64_t*) p);
	float    flt = *((float   *) p);
	double   dbl = *((double  *) p);

	if (st->do_swap) {
		swap2(&i16);
		swap2(&u16);
		swap4(&i32);
		swap4(&u32);
		swap8(&i64);
		swap8(&u64);
		swap4(&flt);
		swap8(&dbl);
	}

	move(0, 0);
	printw("[0x%08x, %c]   flt=",
			st->cursor,
			st->do_swap ? 'X' : '-');
	attron(st->attr); printw("%' -17g", flt); attroff(st->attr);
	printw("   dbl="); attron(st->attr); printw("%lg", dbl); attroff(st->attr);
	clrtoeol();

	move(1, 0);
	printw(   "i8="); attron(st->attr); printw("%' -4.1hhd", i8); attroff(st->attr);
	printw( "  u8="); attron(st->attr); printw("%' -4.1hhu", u8); attroff(st->attr);
	printw("  i16="); attron(st->attr); printw("%' -6.1hd", i16); attroff(st->attr);
	printw("  u16="); attron(st->attr); printw("%' -6.1hu", u16); attroff(st->attr);
	printw("  i32="); attron(st->attr); printw("%' -11.1d",  i32); attroff(st->attr);
	printw("  u32="); attron(st->attr); printw("%' -11.1u",  u32); attroff(st->attr);
	clrtoeol();

	move(2, 0);
	printw("i64="); attron(st->attr); printw("%' -26.1ld", i64); attroff(st->attr);
	printw("u64="); attron(st->attr); printw("%' -26.1lu", u64); attroff(st->attr);
	clrtoeol();

	st->header_lines = 4;
}

void display (binviewer_state_t* st)
{
	erase();
	getmaxyx(stdscr, st->height, st->width);

	st->bytes_per_line = 1;
	st->chars_per_byte = 3;  // "FF "
	while (st->bytes_per_line*2 < (st->width+1)/(st->chars_per_byte))
		st->bytes_per_line *= 2;

	display_data(st);

	unsigned char* p = st->buf + st->page_cursor;
	for (int j = 0; j < st->height; ++j)
	for (int i = 0; i < st->bytes_per_line; ++i) {
		if (st->page_cursor + i+j*st->bytes_per_line >= st->size)
			return;
		mvprintw(j+st->header_lines, i*st->chars_per_byte, "%02x ", *p);
		++p;
	}
}

void move_cursor (binviewer_state_t* st)
{
	int cx = st->cursor-st->page_cursor;
	int cy = cx/st->bytes_per_line;
	cx -= cy*st->bytes_per_line;
	mvprintw(cy+st->header_lines, cx*st->chars_per_byte, "");
}

void handle_keypress (binviewer_state_t* st, int key)
{
	switch (key) {
	case 'l': case KEY_RIGHT:
		++st->cursor;
		break;
	case 'h': case KEY_LEFT:
		--st->cursor;
		break;
	case 'j': case KEY_DOWN:
		st->cursor += st->bytes_per_line;
		break;
	case 'k': case KEY_UP:
		st->cursor -= st->bytes_per_line;
		break;
	case 'J': case KEY_NPAGE:
		st->cursor += st->bytes_per_line * (st->height-st->header_lines);
		break;
	case 'K': case KEY_PPAGE:
		st->cursor -= st->bytes_per_line * (st->height-st->header_lines);
		break;
	case 'H': case KEY_HOME:
		st->cursor = 0;
		break;
	case 'L': case KEY_END:
		st->cursor = st->size-1;
		break;
	case 's':
		st->do_swap = !st->do_swap;
		break;
	case 'q':
		st->running = false;
		break;
	}
}

void ajust_cursor (binviewer_state_t* st)
{
	if (st->cursor >= st->size)
		st->cursor = st->size-1;
	if (st->cursor < 0)
		st->cursor = 0;

	if (st->page_cursor > st->cursor)
		st->page_cursor = st->cursor;
	else while (st->cursor - st->page_cursor > st->bytes_per_line*(st->height-st->header_lines-1))
		++st->page_cursor;
}

void binviewer (void* buf, off_t size)
{
	binviewer_state_t state, *st = &state;
	st->buf = (unsigned char*) buf;
	st->size = size;
	st->page_cursor = 0;
	st->cursor = 0;
	st->running = true;
	st->do_swap = false;

	initscr();
	keypad(stdscr, TRUE);

	st->attr = A_BOLD;
	if (has_colors()) {
		start_color();
		init_pair(1, COLOR_YELLOW, COLOR_BLACK);
		st->attr |= COLOR_PAIR(1);
	}

	while(st->running) {
		display(st);
		move_cursor(st);
		refresh();
		handle_keypress(st, getch());
		ajust_cursor(st);
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

