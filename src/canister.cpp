#include <canister.h>

int main() {
	auto server = canister::http::http_server();
	server.run();
}
