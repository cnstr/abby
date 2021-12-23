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

				auto data = bearer == authorization ? "authorized" : "unauthorized";
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
				if (status == "unauthorized") {
					ws->end(401);
				}
			},

			.message = [](uWS::WebSocket<false, true, std::string> *ws, std::string_view message, uWS::OpCode code) {
				// We only need to handle text, everything else can be ignored
				if (code != uWS::TEXT) {
					return;
				}

				// Refresh command
				if (message == "refresh") {
					try {
						canister::log::info("http", "fetching repository manifest");
						auto manifest = canister::http::manifest();
						if (!manifest.has_value()) {
							ws->send("fail:refresh", uWS::TEXT);
							return;
						}

						canister::parser::parse_manifest(manifest.value(), ws);
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

std::optional<nlohmann::json> canister::http::manifest() {
	try {
		curlpp::Easy request;
		std::ostringstream response_stream;

		request.setOpt(new curlpp::options::LowSpeedLimit(0));
		request.setOpt(curlpp::options::Url(MANIFEST_URL));
		request.setOpt(curlpp::options::HttpHeader(canister::http::headers()));
		request.setOpt(curlpp::options::WriteStream(&response_stream));

		request.perform();

		auto http_code = curlpp::infos::ResponseCode::get(request);
		if (http_code != 200) {
			throw new std::runtime_error("invalid status code: " + http_code);
		}

		std::string response = response_stream.str();
		nlohmann::json data = nlohmann::json::parse(response);
		return data;
	} catch (...) {
		return std::nullopt;
	}
}

std::optional<std::ostringstream> canister::http::fetch(const std::string url) {
	try {
		curlpp::Easy request;
		std::ostringstream response_stream;

		// request.setOpt(new curlpp::options::Timeout(10));
		request.setOpt(new curlpp::options::LowSpeedLimit(0));
		request.setOpt(new curlpp::options::Url(url));
		request.setOpt(new curlpp::options::HttpHeader(canister::http::headers()));
		request.setOpt(new curlpp::options::WriteStream(&response_stream));

		request.perform();

		auto http_code = curlpp::infos::ResponseCode::get(request);
		if (http_code != 200) {
			throw new std::runtime_error("invalid status code: " + http_code);
		}

		return response_stream;
	} catch (...) {
		return std::nullopt;
	}
}

std::optional<std::ostringstream> canister::http::sileo_endpoint(const std::string uri) {
	try {
		curlpp::Easy request;
		std::ostringstream response_stream;

		request.setOpt(new curlpp::options::Timeout(10));
		request.setOpt(new curlpp::options::LowSpeedLimit(0));
		request.setOpt(new curlpp::options::Url(uri + "/payment_endpoint"));
		request.setOpt(new curlpp::options::HttpHeader(canister::http::headers()));
		request.setOpt(new curlpp::options::WriteStream(&response_stream));

		request.perform();

		auto http_code = curlpp::infos::ResponseCode::get(request);
		if (http_code != 200) {
			throw new std::runtime_error("invalid status code: " + http_code);
		}

		return response_stream;
	} catch (...) {
		return std::nullopt;
	}
}

std::string canister::http::fetch_release(const std::string slug, const std::string uri) {
	try {
		auto response_stream = canister::http::fetch(uri + "/Release");

		std::string file_name = canister::util::safe_fs_name(uri + "/Release");
		std::string file_path = canister::util::cache_path() + file_name;

		if (!response_stream.has_value()) {
			return std::string("cnstr-not-available");
		}

		canister::log::info("http", slug + " - hit: " + uri + "/Release");
		std::string response = response_stream.value().str();
		std::ifstream file(file_path);

		if (file.good()) { // If this is true that means the file exists
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
}

std::string canister::http::fetch_packages(const std::string slug, const std::string uri) {
	std::string files[6] = {
		"Packages.zst",
		"Packages.xz",
		"Packages.bz2",
		"Packages.lzma",
		"Packages.gz",
		"Packages"
	};

	for (auto &repo_file : files) {
		std::string final_path = canister::util::cache_path() + slug + ".Packages";

		try {
			auto response_stream = canister::http::fetch(uri + "/" + repo_file);
			std::string file_name = canister::util::safe_fs_name(uri + "/" + repo_file);
			std::string file_path = canister::util::cache_path() + file_name;

			if (!response_stream.has_value()) {
				continue;
			}

			canister::log::info("http", slug + " - hit: " + uri + "/" + repo_file);
			std::string response = response_stream.value().str();
			std::ifstream file(file_path);

			if (file.good()) { // If this is true that means the file exists
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

			// Decompress (I wish C++ had switch statements this wouldn't look so ugly)
			if (repo_file == "Packages.zst") {
				canister::decompress::zstd(slug, file_path, final_path);
			} else if (repo_file == "Packages.xz") {
				canister::decompress::xz(slug, file_path, final_path);
			} else if (repo_file == "Packages.bz2") {
				canister::decompress::bz2(slug, file_path, final_path);
			} else if (repo_file == "Packages.lzma") {
				canister::decompress::lzma(slug, file_path, final_path);
			} else if (repo_file == "Packages.gz") {
				canister::decompress::gz(slug, file_path, final_path);
			} else if (repo_file == "Packages") {
				std::filesystem::rename(file_path, final_path);
			}

			// Make sure the decompressed file is not empty
			std::ifstream file_check(final_path);
			if (!file_check) {
				continue;
			}

			if (file_check.peek() == std::ifstream::traits_type::eof()) {
				file_check.close();
				std::filesystem::remove(final_path);
				continue;
			}

			file_check.close();
			return final_path;
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
	}

	return std::string("cnstr-not-available");
}

std::string canister::http::fetch_dist_release(const std::string slug, const std::string uri, const std::string dist) {
	auto url = uri + "/dists/" + dist;
	return canister::http::fetch_release(slug, url);
}

std::string canister::http::fetch_dist_packages(const std::string slug, const std::string uri, const std::string dist, const std::string suite) {
	auto url = uri + "/dists/" + dist + "/" + suite + "/binary-iphoneos-arm";
	return canister::http::fetch_packages(slug, url);
}
