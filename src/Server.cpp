#include <canister.h>
#include <uws/App.h>
#include <nlohmann/json.hpp>
#include "static/Canister.hpp"

int main() {
	auto server = uWS::App();
	server.get("/healthz", [](uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
		auto json = nlohmann::json({
			{ "status", "OK" },
			{ "timestamp", Canister::timestamp() }
		});

		res->writeHeader("Content-Type", "application/json");
		res->end(json.dump());
	});

	server.listen(8080, [](auto *socket) {
		if (socket) {
			std::cout << "[cnstr] Running" << std::endl;
		} else {
			std::cerr << "[cnstr] Failed to create Socket" << std::endl;
		}
	});

	server.run();
}
