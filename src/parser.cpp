#include <canister.h>

void canister::parser::parse_manifest(const nlohmann::json data, uWS::WebSocket<false, true, std::string> *ws) {
	canister::log::info("parser", "processing repository manifest");

	int successful = 0;
	int failed = 0;
	int cached = 0;

	std::vector<canister::parser::repo_manifest> manifests;

	for (auto &[iter, value] : data.items()) {
		if (!value.contains("slug") || !value.contains("ranking") || !value.contains("uri")) {
			auto json = nlohmann::json({
				{ "repo", "unknown" },
				{ "type", "failure" },
				{ "message", "missing slug, ranking, or uri" },
				{ "timestamp", canister::util::timestamp() },
			});

			ws->send(json.dump(), uWS::TEXT);
		}

		// Get all of our necessary props from the manifest
		auto manifest = canister::parser::repo_manifest();
		manifest.slug = value["slug"].get<std::string>();
		manifest.ranking = value["ranking"].get<std::int8_t>();
		manifest.uri = value["uri"].get<std::string>();

		// The URI must not have a trailing slash
		if (manifest.uri.ends_with("/")) {
			manifest.uri.pop_back();
		}

		if (value.contains("aliases")) {
			manifest.aliases = value["aliases"].get<std::vector<std::string>>();
		} else {
			manifest.aliases = std::vector<std::string>();
		}

		if (value.contains("dist") && value.contains("suite")) {
			manifest.dist = value["dist"].get<std::string>();
			manifest.suite = value["suite"].get<std::string>();
		} else {
			manifest.dist = "";
			manifest.suite = "";
		}

		manifests.push_back(manifest);
	}

	for (auto &manifest : manifests) {
		std::string release_path, packages_path;
		std::map<std::string, std::string> release;
		canister::parser::packages_info packages_info;

		if (manifest.dist.empty() && manifest.suite.empty()) {
			release_path = canister::http::fetch_release(manifest.slug, manifest.uri);
			packages_path = canister::http::fetch_packages(manifest.slug, manifest.uri);
		} else {
			// TODO: All these methods should only take `manifest` and get the values from there
			release_path = canister::http::fetch_dist_release(manifest.slug, manifest.uri, manifest.dist);
			packages_path = canister::http::fetch_dist_packages(manifest.slug, manifest.uri, manifest.dist, manifest.suite);
		}

		if (release_path == "cnstr-not-available") {
			ws->send("failed:download_release:" + manifest.slug, uWS::TEXT);
			canister::log::error("http", manifest.slug + " - failed to download release");
			failed++;
			continue;
		}

		if (packages_path == "cnstr-not-available") {
			ws->send("failed:download_packages:" + manifest.slug, uWS::TEXT);
			canister::log::error("http", manifest.slug + " - failed to download packages");
			failed++;
			continue;
		}

		if (release_path == "cnstr-cache-available" && packages_path == "cnstr-cache-available") {
			canister::log::info("parser", manifest.slug + " - skipping due to cache");
			cached++;
			continue;
		}

		if (release_path != "cnstr-cache-available") {
			// Read files and parse with the parser
			std::ifstream release_file(release_path);
			if (!release_file.good()) {
				release_file.close();
				canister::log::error("parser", manifest.slug + " - release failed fs check");
				ws->send("failed:parser_release:" + manifest.slug, uWS::TEXT);
				failed++;
				continue;
			}

			std::ostringstream release_stream;
			release_stream << release_file.rdbuf();
			std::string release_contents = release_stream.str();
			release_file.close();

			if (release_contents.empty()) {
				ws->send("failed:parser_release:" + manifest.slug, uWS::TEXT);
				canister::log::error("parser", manifest.slug + " - empty release");
				failed++;
				continue;
			}

			release = canister::parser::parse_release(manifest.slug, release_contents);
		}

		if (packages_path != "cnstr-cache-available") {
			std::ifstream packages_file(packages_path);
			if (!packages_file.good()) {
				packages_file.close();
				canister::log::error("parser", manifest.slug + " - packages failed fs check");
				ws->send("failed:parser_packages:" + manifest.slug, uWS::TEXT);
				failed++;
				continue;
			}

			std::ostringstream packages_stream;
			packages_stream << packages_file.rdbuf();
			std::string packages_contents = packages_stream.str();
			packages_file.close();

			if (packages_contents.empty()) {
				ws->send("failed:parser_packages:" + manifest.slug, uWS::TEXT);
				canister::log::error("parser", manifest.slug + " - empty packages");
				failed++;
				continue;
			}

			packages_info = canister::parser::parse_packages(manifest.slug, packages_contents);
		}

		auto request = canister::http::sileo_endpoint(manifest.uri);
		auto endpoint = request.has_value() ? request.value().str() : "";

		// Ths dist and suite are blank strings because NULL is unacceptable
		canister::db::write_repository({
			.slug = manifest.slug,
			.aliases = manifest.aliases,
			.ranking = manifest.ranking,
			.package_count = packages_info.count,
			.sections = packages_info.sections,
			.uri = manifest.uri,
			.dist = "",
			.suite = "",
			.name = release["Name"],
			.version = release["Version"],
			.description = release["Description"],
			.date = release["Date"],
			.payment_gateway = release["Payment-Gateway"],
			.sileo_endpoint = endpoint,
		});

		// TODO: Support Payment-Gateway specification for price calculation
		for (auto &package_map : packages_info.data) {
			// This means a package with the ID does not exist
			auto exists = canister::db::package_exists(package_map["Package"]);

			std::string price;
			if (package_map["Tag"].find("cydia::commercial") != std::string::npos) {
				if (endpoint.length() > 0) {
					auto sileo_price_request = canister::http::sileo_endpoint_price(package_map["Package"], endpoint);
					price = sileo_price_request.has_value() ? sileo_price_request.value() : "Paid";
				} else {
					price = "Paid";
				}
			} else {
				price = "Free";
			}

			if (!exists.has_value()) {
				canister::db::write_package({
					.id = package_map["Package"],
					.repo = manifest.slug,
					.price = price,
				});

				auto header = package_map["Header"].length() > 0 ? package_map["Header"] : "";
				auto tint_color = "";
				auto udid = package_map["Package"] + "$$" + package_map["Version"] + "$$" + manifest.slug;

				// TODO: Support the new DepictionKit specification
				canister::db::write_vpackage({
					.uuid = udid,
					.package = package_map["Package"],
					.current_version = true,
					.version = package_map["Version"],
					.architecture = package_map["Architecture"],
					.filename = package_map["Filename"],
					.sha_256 = package_map["SHA256"],
					.name = package_map["Name"],
					.description = package_map["Description"],
					.author = package_map["Author"],
					.maintainer = package_map["Maintainer"],
					.depiction = package_map["Depiction"],
					.native_depiction = package_map["SileoDepiction"],
					.header = header,
					.tint_color = tint_color,
					.icon = package_map["Icon"],
					.section = package_map["Section"],
					.tag = package_map["Tag"],
					.installed_size = package_map["Installed-Size"],
					.size = package_map["Size"],
				});
			} else {
				// Update the package's repository if the newer slug has a better ranking
				auto db_slug = exists.value();
				auto db_ranking = canister::db::repository_ranking(db_slug);

				// Lower ranking is better
				if (manifest.ranking < db_ranking) {
					canister::db::write_package({
						.id = package_map["Package"],
						.repo = manifest.slug,
						.price = price,
					});
				}

				// Insert this subpackage as a VPackage and set current_version using version comparison.
				auto vpackage_query = canister::db::current_vpackage_version(package_map["Package"]);
				if (!vpackage_query.has_value()) {
					continue;
				}

				auto header = package_map["Header"].length() > 0 ? package_map["Header"] : "";
				auto tint_color = "";
				auto udid = package_map["Package"] + "$$" + package_map["Version"] + "$$" + manifest.slug;

				canister::db::write_vpackage({
					.uuid = udid,
					.package = package_map["Package"],
					.current_version = false,
					.version = package_map["Version"],
					.architecture = package_map["Architecture"],
					.filename = package_map["Filename"],
					.sha_256 = package_map["SHA256"],
					.name = package_map["Name"],
					.description = package_map["Description"],
					.author = package_map["Author"],
					.maintainer = package_map["Maintainer"],
					.depiction = package_map["Depiction"],
					.native_depiction = package_map["SileoDepiction"],
					.header = header,
					.tint_color = tint_color,
					.icon = package_map["Icon"],
					.section = package_map["Section"],
					.tag = package_map["Tag"],
					.installed_size = package_map["Installed-Size"],
					.size = package_map["Size"],
				});

				// When it's equal to a value of one that means the first argument is a greater version
				if (canister::dpkg::compare(package_map["Version"], vpackage_query.value()) == 1) {
					canister::db::set_current_vpackage(udid, package_map["Package"]);
				}
			}
		}

		successful++;

		ws->send("success:" + std::to_string(successful), uWS::OpCode::TEXT);
		ws->send("failed:" + std::to_string(failed), uWS::OpCode::TEXT);
		ws->send("cached:" + std::to_string(cached), uWS::OpCode::TEXT);
	}
}

canister::parser::packages_info canister::parser::parse_packages(const std::string id, const std::string content) {
	size_t start, end = 0;
	canister::parser::packages_info info{};
	std::vector<std::future<std::map<std::string, std::string>>> package_threads;

	while ((start = content.find_first_not_of("\n\n", end)) != std::string::npos) {
		end = content.find("\n\n", start);

		// We want to do each package on a separate thread (hopefully BigBoss plays nicely)
		package_threads.push_back(std::async(std::launch::async, canister::parser::parse_apt_kv, std::stringstream(content.substr(start, end - start)), canister::util::packages_keys()));
	}

	for (auto &result : package_threads) {
		result.wait();
		auto package = result.get();
		info.data.push_back(package);

		// Check if the section isn't already in the array then adds it
		if (!package.contains("Section")) {
			continue;
		}

		auto search_result = std::find(info.sections.begin(), info.sections.end(), package["Section"]);
		if (search_result == info.sections.end()) {
			info.sections.push_back(package["Section"]);
		}
	}

	info.count = info.data.size();
	canister::log::info("parser", id + " - packages count: " + std::to_string(info.count));
	return info;
}

std::map<std::string, std::string> canister::parser::parse_release(const std::string id, const std::string content) {
	const auto release = parse_apt_kv(std::stringstream(content), canister::util::release_keys());
	canister::log::info("parser", id + " - key length: " + std::to_string(release.size()));
	return release;
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
			} else {
				previous_key = matches[1];
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
