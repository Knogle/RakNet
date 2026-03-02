#include <cstdio>

namespace
{
	void PrintUsage()
	{
		std::fprintf(stderr, "Usage: omp_connect_probe <family> <host> <port>\n");
		std::fprintf(stderr, "  family: ipv4 | ipv6\n");
	}
}

int main(int argc, char**)
{
	if (argc != 4)
	{
		PrintUsage();
		return 1;
	}

	std::fprintf(stderr, "omp_connect_probe: handshake probe not implemented yet\n");
	return 2;
}
