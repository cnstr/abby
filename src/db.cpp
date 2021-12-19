#include <canister.h>

void canister::db::bootstrap() {
	auto stream = std::ostringstream();
	std::ifstream file("./bootstrap.sql");

	if (!file.is_open() || !file.good()) {
		canister::log::error("db", "mangled bootstrap file");
		return;
	}

	stream << file.rdbuf();
	auto contents = stream.str();

	const auto connection = tao::pq::connection::create(std::getenv("DB_CONN"));

	size_t start, end = 0;
	while ((start = contents.find_first_not_of("---", end)) != std::string::npos) {
		end = contents.find("---", start);
		auto statement = contents.substr(start, end - start);
		connection->execute(tao::pq::internal::zsv(statement));
	}
}
