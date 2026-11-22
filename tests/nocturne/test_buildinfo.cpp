#include <cassert>
#include <sstream>

#include "buildinfo.hpp"

int main()
{
    std::istringstream release("Nocturne v1.0.4\n0e7cfd5\n");
    const auto build = readBuildInfo(release);
    assert(build.version == "Nocturne v1.0.4");
    assert(build.revision == "0e7cfd5");

    std::istringstream windows("Nocturne v1.0.3\r\ned70952\r\n");
    const auto previous = readBuildInfo(windows);
    assert(previous.version == "Nocturne v1.0.3");
    assert(previous.revision == "ed70952");

    std::istringstream legacy("Nocturne");
    assert(readBuildInfo(legacy).revision == "Unknown");

    std::istringstream missing("");
    const auto unknown = readBuildInfo(missing);
    assert(unknown.version == "Unknown" && unknown.revision == "Unknown");
}
