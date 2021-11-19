#define PROJECT_NAME "canister"
#define UWS_NO_ZLIB 1 // Disable compression on uWebSockets
#define UWS_HTTPRESPONSE_NO_WRITEMARK // Remove the uWebSockets header

#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <uws/App.h>

namespace canister {
	class util {
	public:
		static std::string timestamp();
	};

	class log {
	public:
		static void info(const std::string location, const std::string message);
		static void error(const std::string location, const std::string message);
	};

	class http {
	public:
		static uWS::App http_server();
		static void fetch_release(const std::string repo_url);
		static void fetch_packages(const std::string repo_url);
		static void fetch_dist_release(const std::string repo_url, const std::string dist_name);
		static void fetch_dist_packages(const std::string repo_url, const std::string dist_name, const std::string suite_name);
	};

	class compress {
	public:
		static void gz_extract(const std::string archive_path, const std::string cache_path);
		static void xz_extract(const std::string archive_path, const std::string cache_path);
		static void bz2_extract(const std::string archive_path, const std::string cache_path);
		static void lzma_extract(const std::string archive_path, const std::string cache_path);
		static void zstd_extract(const std::string archive_path, const std::string cache_path);
	};
};
