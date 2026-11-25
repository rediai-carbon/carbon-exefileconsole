#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <signal.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	char path[PATH_MAX];
	ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if (length <= 0)
	{
		return 90;
	}
	path[length] = '\0';
	std::printf("PATH=%s\n", path);
	for (int i = 0; i < argc; ++i)
	{
		std::printf("ARGV%d=%s\n", i, argv[i]);
	}
	const char* inherit = std::getenv("EXEFILE_INHERIT");
	std::printf("INHERIT=%s\n", inherit ? inherit : "<unset>");
	const char* propagated = std::getenv("TEST_PROPAGATE");
	std::printf("PROP=%s\n", propagated ? propagated : "<unset>");

	char input[128];
	ssize_t got = ::read(STDIN_FILENO, input, sizeof(input) - 1);
	if (got < 0)
	{
		got = 0;
	}
	input[got] = '\0';
	std::printf("STDIN=%s\n", input);
	std::fflush(stdout);
	std::fprintf(stderr, "STUB_ERR\n");
	std::fflush(stderr);

	for (int i = 1; i < argc; ++i)
	{
		if (std::strncmp(argv[i], "--exit=", 7) == 0)
		{
			return std::atoi(argv[i] + 7);
		}
		if (std::strncmp(argv[i], "--signal=", 9) == 0)
		{
			raise(std::atoi(argv[i] + 9));
			return 91;
		}
	}
	return 0;
}
