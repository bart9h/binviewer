#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <curses.h>

void display (void* start, off_t count)
{
	int height, width;
	getmaxyx(stdscr, height, width);

	int bytes_per_line = 1;
	const int chars_per_byte = 3;  // "FF "
	while (bytes_per_line*2 < (width+1)/(chars_per_byte))
		bytes_per_line *= 2;

	unsigned char* p = (unsigned char*) start;
	for (int j = 0; j < height; ++j)
	for (int i = 0; i < bytes_per_line; ++i) {
		mvprintw(j, i*chars_per_byte, "%02x ", *p);
		++p;
		if (--count == 0)
			return;
	}
}

void binviewer (void* buf, off_t size)
{
	initscr();
	keypad(stdscr, TRUE);

	int running = 1;
	while(running) {
		display(buf, size);
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

