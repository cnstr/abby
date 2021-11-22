#include <canister.h>

int main() {
	try {
		auto server = canister::http::http_server();
		server.run();
	} catch (std::exception &exc) {
		canister::log::error("http", exc.what());
	}
}
