#define CATCH_CONFIG_RUNNER
#include <catch.hpp>
#include <conio.h>

int main(int argc, char* argv[])
{
	int result = Catch::Session().run(argc, argv);

	if (IsDebuggerPresent())
	{
		printf("Press any key to continue...\n");
		while (_kbhit() == 0);
	}

	return (result < 0xff ? result : 0xff);
}
