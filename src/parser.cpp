#include <canister.h>

void canister::parser::parse_manifest(const nlohmann::json data, uWS::WebSocket<false, true, std::string> *ws) {
	canister::log::info("parser", "processing repository manifest");
	for (const auto &repository : data.items()) {
		auto object = repository.value();
		auto message = std::string("success:") + object["slug"].get<std::string>();
		ws->send(message, uWS::OpCode::TEXT);

		for (const auto &entry : repository.value().items()) {
			// TODO: Parse and delegate
		}
	}
}

void canister::parser::parse_packages(const std::string id, const std::string content) {
	size_t start, end = 0;
	std::vector<std::map<std::string, std::string>> packages;
	std::vector<std::future<std::map<std::string, std::string>>> package_threads;

	while ((start = content.find_first_not_of("\n\n", end)) != std::string::npos) {
		end = content.find("\n\n", start);

		// We want to do each package on a separate thread (hopefully BigBoss plays nicely) // TODO: Prettify
		package_threads.push_back(std::async(std::launch::async, canister::parser::parse_apt_kv, std::stringstream(content.substr(start, end - start))));
	}

	for (auto &result : package_threads) {
		packages.push_back(result.get());
	}

	canister::log::info("parser", id + " - packages count: " + std::to_string(packages.size()));
	// TODO: Write to Database
}

void canister::parser::parse_release(const std::string id, const std::string content) {
	const auto release = parse_apt_kv(std::stringstream(content));
	// TODO: Write to Database
}

std::map<std::string, std::string> canister::parser::parse_apt_kv(std::stringstream stream) {
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

		kv_map.insert(std::make_pair(matches[1], matches[2]));
		previous_key = matches[1];
	}

	return kv_map;
}
