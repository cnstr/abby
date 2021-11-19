#include <canister.h>

void canister::log::info(const std::string location, const std::string message) {
	std::cout << "[i] (" << location << "): " << message << std::endl;
}

void canister::log::error(const std::string location, const std::string message) {
	std::cerr << "[x] (" << location << "): " << message << std::endl;
}

