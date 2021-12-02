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

				auto data = bearer == authorization ? "Authorized" : "Unauthorized";
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

				// Refresh command
				if (message == "refresh") {
					try {
						canister::log::info("http", "fetching repository manifest");
						ws->send("fetching repository manifest", uWS::OpCode::TEXT);
						auto manifest = canister::http::manifest();
						manifest.wait();

						canister::parser::parse_manifest(manifest.get(), ws);

					} catch (curlpp::LogicError &exc) {
						auto message = "failed to fetch manifest (logic): " + std::string(exc.what());
						canister::log::error("http", message);
					} catch (curlpp::RuntimeError &exc) {
						auto message = "failed to fetch manifest (runtime): " + std::string(exc.what());
						canister::log::error("http", message);
					}
				}
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
			canister::log::info("http", "running successfully");
		} else {
			canister::log::error("http", "failed to bind to socket");
		}
	});

	return server;
};

std::list<std::string> canister::http::headers() {
	std::list<std::string> headers;
	headers.push_back("X-Firmware: 2.0");
	headers.push_back("X-Machine: iPhone13,1");
	headers.push_back("Cache-Control: no-cache");
	headers.push_back("X-Unique-ID: canister-v2-unique-device-identifier");
	return headers;
}

std::future<nlohmann::json> canister::http::manifest() {
	return std::async(std::launch::async, []() {
		curlpp::Easy request;
		std::ostringstream response_stream;

		request.setOpt(curlpp::options::Timeout(10));
		request.setOpt(new curlpp::options::Url(MANIFEST_URL));
		request.setOpt(new curlpp::options::HttpHeader(canister::http::headers()));
		request.setOpt(new curlpp::options::WriteStream(&response_stream));

		request.perform();
		std::string response = response_stream.str();
		nlohmann::json data = nlohmann::json::parse(response);
		return data;
	});
}

std::ostringstream canister::http::fetch(const std::string url) {
	curlpp::Easy request;
	curlpp::Cleanup cleanup;
	std::ostringstream response_stream;

	request.setOpt(new curlpp::options::Timeout(10));
	request.setOpt(new curlpp::options::Url(url));
	request.setOpt(new curlpp::options::HttpHeader(canister::http::headers()));
	request.setOpt(new curlpp::options::WriteStream(&response_stream));

	request.perform();

	auto http_code = curlpp::infos::ResponseCode::get(request);
	if (http_code != 200) {
		throw new std::runtime_error("invalid status code: " + http_code);
	}

	return response_stream;
}

std::future<std::string> canister::http::fetch_release(const std::string slug, const std::string uri) {
	return std::async(std::launch::async, [slug, uri]() {
		try {
			auto response_stream = canister::http::fetch(uri + "/Release");

			std::string file_name = canister::util::safe_fs_name(uri + "/Release");
			std::string file_path = canister::util::cache_path() + file_name;
			std::string response = response_stream.str();
			std::ifstream file(file_path);

			if (file.is_open()) { // If this is true that means the file exists
				// Converts our ifstream to a string using streambuf iterators
				std::string cached = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
				if (canister::util::matched_hash(response, cached)) {
					return std::string("cnstr-cache-available");
				}
			}

			// Write the response data to the file and then return the file name
			std::ofstream out(file_path, std::ios::binary | std::ios::out);
			out << response;
			out.flush();
			out.close();

			return std::string(file_path);
		} catch (curlpp::LogicError &exc) {
			canister::log::error("http", slug + " - curl logic error: " + std::string(exc.what()));
			return std::string("cnstr-not-available");
		} catch (curlpp::RuntimeError &exc) {
			canister::log::error("http", slug + " - curl runtime error: " + std::string(exc.what()));
			return std::string("cnstr-not-available");
		} catch (std::exception &exc) {
			auto message = slug + " - exception: " + std::string(exc.what());
			canister::log::error("http", message);

			sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_ERROR, "http", message.c_str()));
			return std::string("cnstr-not-available");
		}
	});
}

std::future<std::string> canister::http::fetch_packages(const std::string slug, const std::string uri) {
	return std::async(std::launch::async, [slug, uri]() {
		std::string files[6] = {
			"Packages.zst",
			"Packages.xz",
			"Packages.bz2",
			"Packages.lzma",
			"Packages.gz",
			"Packages"
		};

		for (auto &file : files) {
			auto task = std::async(std::launch::async, [file, slug, uri]() {
				try {
					auto response_stream = canister::http::fetch(uri + "/" + file);

					std::string file_name = canister::util::safe_fs_name(uri + "/" + file);
					std::string file_path = canister::util::cache_path() + file_name;
					std::string response = response_stream.str();
					std::ifstream file(file_path);

					if (file.is_open()) { // If this is true that means the file exists
						// Converts our ifstream to a string using streambuf iterators
						std::string cached = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
						if (canister::util::matched_hash(response, cached)) {
							return std::string("cnstr-cache-available");
						}
					}

					// Write the response data to the file and then return the file name
					std::ofstream out(file_path, std::ios::binary | std::ios::out);
					out << response;
					out.flush();
					out.close();

					return std::string(file_path);
				} catch (curlpp::LogicError &exc) {
					canister::log::error("http", slug + " - curl logic error: " + std::string(exc.what()));
					return std::string("cnstr-not-available");
				} catch (curlpp::RuntimeError &exc) {
					canister::log::error("http", slug + " - curl runtime error: " + std::string(exc.what()));
					return std::string("cnstr-not-available");
				} catch (std::exception &exc) {
					auto message = slug + " - exception: " + std::string(exc.what());
					canister::log::error("http", message);

					sentry_capture_event(sentry_value_new_message_event(SENTRY_LEVEL_ERROR, "http", message.c_str()));
					return std::string("cnstr-not-available");
				}
			});

			task.wait();
		}

		return std::string("cnstr-not-available");
	});
}
