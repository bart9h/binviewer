#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <curses.h>

void binviewer (void* buf, off_t size)
{
	initscr();
	printw("Hello World !!!");
	refresh();
	getch();
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

