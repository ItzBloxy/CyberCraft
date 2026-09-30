#include "Harness.h"

int main()
{
	for (const auto& test : th::Registry())
	{
		const int before = th::Failures();
		test.fn();
		std::printf("%s %s\n", th::Failures() == before ? "PASS" : "FAIL", test.name);
	}
	std::printf("%zu test(s), %d failure(s)\n", th::Registry().size(), th::Failures());
	return th::Failures() == 0 ? 0 : 1;
}
