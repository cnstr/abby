#include <canister.h>

uWS::App canister::http::http_server() {
	auto server = uWS::App();

	const auto token = std::getenv("GATEWAY_BEARER");
	if (!token) {
		throw std::runtime_error("env 'GATEWAY_BEARER' not set");
	}

	const auto bearer = std::string(token);

	// Canister's refresh commands will be sent via a WebSocket
	server.ws<std::string>("/",
		{
			.compression = uWS::DISABLED,
			.maxPayloadLength = 8 * 1024, // This is pretty much a command only socket so the size can be small
			.maxBackpressure = 32 * 1024, // Theoretically we should never even be hitting this limit

			.upgrade = [bearer](uWS::HttpResponse<false> *res, uWS::HttpRequest *req, us_socket_context_t *context) {

				const auto authorization = req->getHeader("sec-authorization");

				auto data = bearer == authorization ? "Authorized": "Unauthorized";
				auto key = req->getHeader("sec-websocket-key");
				auto protocol = req->getHeader("sec-websocket-protocol");
				auto extensions = req->getHeader("sec-websocket-extensions");
				res->upgrade<std::string>(data, key, protocol, extensions, context);
			},

			.open = [](uWS::WebSocket<false, true, std::string> *ws) {
				const auto status = static_cast<std::string>(*ws->getUserData());

				auto json = nlohmann::json({
					{ "status", status },
					{ "timestamp", canister::util::timestamp() },
				});

				ws->send(json.dump(), uWS::TEXT);
				if (status == "Unauthorized") {
					ws->end(401);
				}
			},

			.message = [](uWS::WebSocket<false, true, std::string> *ws, std::string_view message, uWS::OpCode code) {
				// We only need to handle text, everything else can be ignored
				if (code != uWS::OpCode::TEXT) {
					return;
				}

				ws->send(message);
			},
		});

	server.get("/healthz", [](uWS::HttpResponse<false> *res, __attribute__((unused)) uWS::HttpRequest *req) {
		auto json = nlohmann::json({
			{ "status", "200 OK" },
			{ "timestamp", canister::util::timestamp() },
		});

		res->cork([res, json]() {
			res->writeHeader("Content-Type", "application/json");
			res->writeStatus("200 OK");
			res->end(json.dump());
		});
	});

	// uws tries globstar methods last so we can use it to catch 404s
	server.any("/*", [](uWS::HttpResponse<false> *res, __attribute__((unused)) uWS::HttpRequest *req) {
		auto json = nlohmann::json({
			{ "status", "404 Not Found" },
			{ "timestamp", canister::util::timestamp() },
		});

		res->cork([res, json]() {
			res->writeHeader("Content-Type", "application/json");
			res->writeStatus("404 Not Found");
			res->end(json.dump());
		});
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
