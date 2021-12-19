#include <canister.h>

int main() {
	const auto connection = std::getenv("DB_CONN");
	if (!connection) {
		canister::log::error("db", "connection string DB_CONN not set");
		return 1;
	}

	// Database Migrations
	canister::db::bootstrap();

	sentry_options_t *options = sentry_options_new();
	sentry_options_set_dsn(options, SENTRY_DSN);
	sentry_init(options);

	// This directory is used for cache
	if (!std::filesystem::is_directory("/tmp/canister") || !std::filesystem::exists("/tmp/canister")) {
		std::filesystem::create_directory("/tmp/canister");
	}

	try {
		auto server = canister::http::http_server();
		server.run();
	} catch (std::exception &exc) {
		canister::log::error("http", exc.what());
	}

	sentry_close();
}
