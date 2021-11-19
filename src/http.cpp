#include <canister.h>

uWS::App canister::http::http_server() {
	auto server = uWS::App();
	server.get("/healthz", [](uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
		auto json = nlohmann::json({
			{ "status", "OK" },
			{ "timestamp", canister::util::timestamp() },
		});

		res->writeHeader("Content-Type", "application/json");
		res->end(json.dump());
	});

	server.listen(8080, [](auto *socket) {
		if (socket) {
			canister::log::info("http_server", "running successfully");
		} else {
			canister::log::error("http_server", "failed to bind to socket");
		}
	});

	return server;
};
