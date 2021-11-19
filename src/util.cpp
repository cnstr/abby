#include <canister.h>

std::string canister::util::timestamp() {
	const auto now = std::chrono::system_clock::now();
	return std::to_string(now.time_since_epoch().count());
}
