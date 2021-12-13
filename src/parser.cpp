#include <canister.h>

void canister::parser::parse_manifest(const nlohmann::json data, uWS::WebSocket<false, true, std::string> *ws) {
	canister::log::info("parser", "processing repository manifest");
	std::vector<std::future<void>> tasks;
	int success = 0, failed = 0, cached = 0;

	for (const auto &repository : data.items()) {
		auto object = repository.value();

		auto slug = object["slug"].get<std::string>();
		auto ranking = object["ranking"].get<std::int8_t>();
		auto uri = object["uri"].get<std::string>();

		// The URI needs to not have a trailing slash
		if (uri.ends_with("/")) {
			uri.pop_back();
		}

		// Distribution repository
		if (object.contains("dist") && object.contains("suite")) {
			auto dist = object["dist"].get<std::string>();
			auto suite = object["suite"].get<std::string>();

			continue;
		}

		std::string release_path = canister::http::fetch_release(slug, uri);
		if (release_path == "cnstr-not-available") {
			ws->send("failed:download_release:" + slug, uWS::TEXT);
			failed++;
			continue;
		}

		std::string packages_path = canister::http::fetch_packages(slug, uri);
		if (packages_path == "cnstr-not-available") {
			ws->send("failed:download_packages:" + slug, uWS::TEXT);
			failed++;
			continue;
		}

		if (release_path == "cnstr-cache-available" && packages_path == "cnstr-cache-available") {
			cached++;
			continue;
		}

		if (release_path != "cnstr-cache-available") {
			// Read files and parse with the parser
			std::ifstream release_file(release_path);
			if (!release_file.good()) {
				release_file.close();
				canister::log::error("parser", slug + " - release failed fs check");
				ws->send("failed:parser_release:" + slug, uWS::TEXT);
				failed++;
				continue;
			}

			std::ostringstream release_stream;
			release_stream << release_file.rdbuf();
			std::string release_contents = release_stream.str();
			release_file.close();

			if (release_contents.empty()) {
				ws->send("failed:parser_release:" + slug, uWS::TEXT);
				canister::log::error("parser", slug + " - empty release");
				failed++;
				continue;
			}

			canister::parser::parse_release(slug, release_contents);
		}

		if (packages_path != "cnstr-cache-available") {
			std::ifstream packages_file(packages_path);
			if (!packages_file.good()) {
				packages_file.close();
				canister::log::error("parser", slug + " - packages failed fs check");
				ws->send("failed:parser_packages:" + slug, uWS::TEXT);
				failed++;
				continue;
			}

			std::ostringstream packages_stream;
			packages_stream << packages_file.rdbuf();
			std::string packages_contents = packages_stream.str();
			packages_file.close();

			if (packages_contents.empty()) {
				ws->send("failed:parser_packages:" + slug, uWS::TEXT);
				canister::log::error("parser", slug + " - empty packages");
				failed++;
				continue;
			}

			canister::parser::parse_packages(slug, packages_contents);
		}

		success++;
	}

	ws->send("success:" + std::to_string(success), uWS::OpCode::TEXT);
	ws->send("failed:" + std::to_string(failed), uWS::OpCode::TEXT);
	ws->send("cached:" + std::to_string(cached), uWS::OpCode::TEXT);
}

void canister::parser::parse_packages(const std::string id, const std::string content) {
	size_t start, end = 0;
	std::vector<std::map<std::string, std::string>> packages;
	std::vector<std::future<std::map<std::string, std::string>>> package_threads;

	while ((start = content.find_first_not_of("\n\n", end)) != std::string::npos) {
		end = content.find("\n\n", start);

		// We want to do each package on a separate thread (hopefully BigBoss plays nicely) // TODO: Prettify
		package_threads.push_back(std::async(std::launch::async, canister::parser::parse_apt_kv, std::stringstream(content.substr(start, end - start)), canister::util::packages_keys()));
	}

	for (auto &result : package_threads) {
		result.wait();
		packages.push_back(result.get());
	}

	canister::log::info("parser", id + " - packages count: " + std::to_string(packages.size()));
	// TODO: Write to Database
}

void canister::parser::parse_release(const std::string id, const std::string content) {
	const auto release = parse_apt_kv(std::stringstream(content), canister::util::release_keys());
	canister::log::info("parser", id + " - key length: " + std::to_string(release.size()));
	// TODO: Write to Database
}

std::map<std::string, std::string> canister::parser::parse_apt_kv(std::stringstream stream, std::vector<std::string> key_validator) {
	std::map<std::string, std::string> kv_map;
	std::string line, previous_key;

	while (std::getline(stream, line, '\n')) {
		if (line.size() == 0) {
			continue;
		}

		std::smatch matches;
		if (!std::regex_match(line, matches, std::regex("^(.*?): (.*)"))) {
			// There's a chance instead of multiline, some idiot gave a key without value
			// Trim the string incase there may be a space after the colon
			size_t end = line.find_last_not_of(' ');
			end == std::string::npos ? line = "" : line = line.substr(0, end + 1);

			if (!line.ends_with(":")) {
				kv_map[previous_key].append("\n" + line);
			}
		}

		// This means we don't have a value before and after the colon in the KV
		if (matches.size() != 3) {
			continue;
		}

		// Validate our key before adding it to the map since Canister doesn't need all keys
		if (std::find(key_validator.begin(), key_validator.end(), matches[1]) != key_validator.end()) {
			kv_map.insert(std::make_pair(matches[1], matches[2]));
		}

		previous_key = matches[1];
	}

	return kv_map;
}
