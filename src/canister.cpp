#include <canister.h>

int main() {
	const auto connection = std::getenv("DB_CONN");
	if (!connection) {
		canister::log::error("db", "connection string DB_CONN not set");
		return 1;
	}

	canister::db::bootstrap();

	sentry_options_t *options = sentry_options_new();
	sentry_options_set_dsn(options, SENTRY_DSN);
	sentry_init(options);

	try {
		auto server = canister::http::http_server();
		server.run();
	} catch (std::exception &exc) {
		canister::log::error("http", exc.what());
	}

	sentry_close();
}
