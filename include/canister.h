#define PROJECT_NAME "canister"
#define UWS_NO_ZLIB 1 // Disable compression on uWebSockets
#define UWS_HTTPRESPONSE_NO_WRITEMARK // Remove the uWebSockets header

#include <chrono>
#include <fstream>
#include <future>
#include <regex>
#include <sstream>
#include <string>
#include <thread>

#include <bzlib.h>
#include <lzma.h>
#include <nlohmann/json.hpp>
#include <uws/App.h>
#include <zlib.h>
#include <zstd.h>

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
		void fetch_release(const std::string repo_url);
		void fetch_packages(const std::string repo_url);
		void fetch_dist_release(const std::string repo_url, const std::string dist_name);
		void fetch_dist_packages(const std::string repo_url, const std::string dist_name, const std::string suite_name);
	}

	namespace log {
		void info(const std::string location, const std::string message);
		void error(const std::string location, const std::string message);
	}

	namespace parser {
		void parse_packages(const std::string id, const std::string content);
		void parse_release(const std::string id, const std::string content);
		std::map<std::string, std::string> parse_apt_kv(std::stringstream stream);
	}

	namespace util {
		std::string timestamp();
	}
}
