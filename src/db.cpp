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
		INSERT INTO "Repositories" (
			slug,
			aliases,
			ranking,
			package_count,
			sections,
			uri,
			dist,
			suite,
			name,
			version,
			description,
			date,
			payment_gateway,
			sileo_endpoint
		) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14) ON CONFLICT (slug)
		DO UPDATE SET
			aliases=$2,
			ranking=$3,
			package_count=$4,
			sections=$5,
			uri=$6,
			dist=$7,
			suite=$8,
			name=$9,
			version=$10,
			description=$11,
			date=$12,
			payment_gateway=$13,
			sileo_endpoint=$14
		)"""";

	try {
		transaction->execute(statement, data.slug, data.aliases, data.ranking, data.package_count, data.sections, data.uri, data.dist, data.suite, data.name, data.version, data.description, data.date, data.payment_gateway, data.sileo_endpoint);
		transaction->commit();
	} catch (std::exception &exc) {
		canister::log::error("db", exc.what());
		transaction->rollback();
	}
	canister::log::info("db", "inserted_release: " + data.slug);
}
