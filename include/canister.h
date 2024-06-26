#define PROJECT_NAME "canister"
#define UWS_NO_ZLIB 1 // Disable compression on uWebSockets
#define UWS_HTTPRESPONSE_NO_WRITEMARK // Remove the uWebSockets header
#define MANIFEST_URL "https://pull.canister.me/index-repositories.json"
#define USER_AGENT "Canister/2.0 [Core] (+https://canister.me/go/ua)"
#define SENTRY_DSN "https://2493ed76073e4cecb7738191e7e18fc8@o1033514.ingest.sentry.io/6090078"

#include <chrono>
#include <fstream>
#include <future>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

#include <bzlib.h>
#include <ctype.h>
#include <curlpp/Easy.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Multi.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <lzma.h>
#include <nlohmann/json.hpp>
#include <picosha2.h>
#include <sentry.h>
#include <tao/pq.hpp>
#include <uv.h>
#include <uws/App.h>
#include <zlib.h>
#include <zstd.h>

namespace canister {
	namespace db {
		struct repository {
			std::string slug;
			std::vector<std::string> aliases;
			std::int8_t ranking;
			std::uint32_t package_count;
			std::vector<std::string> sections;

			std::string uri;
			std::string dist;
			std::string suite;

			std::string name;
			std::string version;
			std::string description;
			std::string date;
			std::string payment_gateway;
			std::string sileo_endpoint;
		};

		struct package {
			std::string id;
			std::string repo;
			std::string price;
		};

		struct vpackage {
			std::string uuid;
			std::string package;
			bool current_version;

			std::string version;
			std::string architecture;
			std::string filename;

			std::string sha_256;
			std::string name;
			std::string description;
			std::string author;
			std::string maintainer;
			std::string depiction;
			std::string native_depiction;
			std::string header;
			std::string tint_color;
			std::string icon;
			std::string section;
			std::string tag;
			std::string installed_size;
			std::string size;
		};

		void bootstrap();
		std::optional<std::string> package_exists(std::string id);
		std::optional<std::string> current_vpackage_version(std::string package);
		std::int8_t repository_ranking(std::string slug);

		void write_repository(canister::db::repository data);
		void write_package(canister::db::package data);
		void write_vpackage(canister::db::vpackage data);
		void set_current_vpackage(std::string uuid, std::string package);
	}

	namespace decompress {
		void gz(const std::string id, const std::string archive, const std::string cache);
		void xz(const std::string id, const std::string archive, const std::string cache);
		void bz2(const std::string id, const std::string archive, const std::string cache);
		void lzma(const std::string id, const std::string archive, const std::string cache);
		void zstd(const std::string id, const std::string archive, const std::string cache);
	}

	namespace dpkg {
		struct version {
			std::int8_t epoch;
			std::string version;
			std::string revision;
		};

		int compare(const std::string &left, const std::string &right);
		int order(int c);
		int verrevcmp(const char *a, const char *b);
	}

	namespace log {
		void info(const std::string location, const std::string message);
		void error(const std::string location, const std::string message);
	}

	namespace parser {
		struct repo_manifest {
			std::string slug;
			std::int8_t ranking;
			std::vector<std::string> aliases;
			std::string uri;
			std::string dist;
			std::string suite;
		};

		struct packages_info {
			std::uint32_t count;
			std::vector<std::string> sections;
			std::vector<std::map<std::string, std::string>> data;
		};

		void parse_manifest(const nlohmann::json data, uWS::WebSocket<false, true, std::string> *ws);
		std::map<std::string, std::string> parse_release(const std::string id, const std::string content);
		canister::parser::packages_info parse_packages(const std::string id, const std::string content);
		std::map<std::string, std::string> parse_apt_kv(std::stringstream stream, std::vector<std::string> key_validator);
	}

	namespace http {
		uWS::App http_server();
		std::list<std::string> headers();
		std::optional<nlohmann::json> manifest();
		std::optional<std::ostringstream> fetch(const std::string url);
		std::optional<std::ostringstream> sileo_endpoint(const std::string uri);
		std::optional<std::string> sileo_endpoint_price(const std::string package, std::string uri);
		std::string fetch_release(canister::parser::repo_manifest manifest);
		std::string fetch_packages(canister::parser::repo_manifest manifest);
	}

	namespace util {
		std::string timestamp();
		std::string cache_path();
		std::vector<std::string> release_keys();
		std::vector<std::string> packages_keys();
		std::string safe_fs_name(const std::string token);
		bool matched_hash(const std::string left, const std::string right);
	}
}
