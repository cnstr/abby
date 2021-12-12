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
#include <curlpp/Easy.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Multi.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <lzma.h>
#include <nlohmann/json.hpp>
#include <picosha2.h>
#include <sentry.h>
#include <uws/App.h>
#include <zlib.h>
#include <zstd.h>
#include <uv.h>

namespace canister {
	namespace decompress {
		void gz(const std::string id, const std::string archive, const std::string cache);
		void xz(const std::string id, const std::string archive, const std::string cache);
		void bz2(const std::string id, const std::string archive, const std::string cache);
		void lzma(const std::string id, const std::string archive, const std::string cache);
		void zstd(const std::string id, const std::string archive, const std::string cache);
	}

	namespace http {
		uWS::App http_server();
		std::list<std::string> headers();
		std::optional<nlohmann::json> manifest();
		std::optional<std::ostringstream> fetch(const std::string url);
		std::string fetch_release(const std::string slug, const std::string uri);
		std::string fetch_packages(const std::string slug, const std::string uri);
		std::string fetch_dist_release(const std::string repo_url, const std::string dist_name);
		std::string fetch_dist_packages(const std::string repo_url, const std::string dist_name, const std::string suite_name);
	}

	namespace log {
		void info(const std::string location, const std::string message);
		void error(const std::string location, const std::string message);
	}

	namespace parser {
		void parse_manifest(const nlohmann::json data, uWS::WebSocket<false, true, std::string> *ws);
		void parse_packages(const std::string id, const std::string content);
		void parse_release(const std::string id, const std::string content);
		std::map<std::string, std::string> parse_apt_kv(std::stringstream stream);
	}

	namespace util {
		std::string timestamp();
		std::string cache_path();
		std::string safe_fs_name(const std::string token);
		bool matched_hash(const std::string left, const std::string right);
	}
}
