#define PROJECT_NAME "canister"
#define UWS_NO_ZLIB 1 // Disable compression on uWebSockets
#define UWS_HTTPRESPONSE_NO_WRITEMARK // Remove the uWebSockets header

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <uws/App.h>

namespace canister {
	namespace util {
		std::string timestamp();
	}

	namespace log {
		void info(const std::string location, const std::string message);
		void error(const std::string location, const std::string message);
	};

	namespace http {
		uWS::App http_server();
		void fetch_release(const std::string repo_url);
		void fetch_packages(const std::string repo_url);
		void fetch_dist_release(const std::string repo_url, const std::string dist_name);
		void fetch_dist_packages(const std::string repo_url, const std::string dist_name, const std::string suite_name);
	};

	namespace compress {
		void gz_extract(const std::string archive_path, const std::string cache_path);
		void xz_extract(const std::string archive_path, const std::string cache_path);
		void bz2_extract(const std::string archive_path, const std::string cache_path);
		void lzma_extract(const std::string archive_path, const std::string cache_path);
		void zstd_extract(const std::string archive_path, const std::string cache_path);
	};
};
