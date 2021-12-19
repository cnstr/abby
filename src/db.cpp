#include <canister.h>

// Global Variable Pragma
auto connection = tao::pq::connection::create(std::getenv("DB_CONN"));

void canister::db::bootstrap() {
	auto stream = std::ostringstream();
	std::ifstream file("./bootstrap.sql");

	if (!file.is_open() || !file.good()) {
		canister::log::error("db", "mangled bootstrap file");
		return;
	}

	stream << file.rdbuf();
	auto contents = stream.str();

	size_t start, end = 0;
	while ((start = contents.find_first_not_of("---", end)) != std::string::npos) {
		end = contents.find("---", start);
		auto statement = contents.substr(start, end - start);
		connection->execute(tao::pq::internal::zsv(statement));
	}
}

void canister::db::write_release(canister::db::release data) {
	auto transaction = connection->transaction();
	auto statement = R""""(
		INSERT INTO "Repositories" (slug, aliases, ranking, uri, dist, suite, name, version, description, date, gateway)
		VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11) ON CONFLICT (slug) DO UPDATE
		SET aliases=$2, ranking=$3, uri=$4, dist=$5, suite=$6, name=$7, version=$8, description=$9, date=$10, gateway=$11
	)"""";

	try {
		transaction->execute(statement, data.slug, data.aliases, data.ranking, data.uri, data.dist, data.suite, data.name, data.version, data.description, data.date, data.gateway);
		transaction->commit();
	} catch (std::exception &exc) {
		std::cout << exc.what() << std::endl;
		transaction->rollback();
	}
	canister::log::info("db", "inserted_release: " + data.slug);
}
