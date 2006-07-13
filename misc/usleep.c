#include <unistd.h>

int main(int argc, char **argv)
{
	int i;

	if (argc < 2)
		return 0;

	i = atoi(argv[1]);
	if (i < 0 || i > (int)60e6)
		return 0;

	usleep(i);

	return 0;
}
