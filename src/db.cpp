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

std::optional<std::string> canister::db::package_exists(std::string id) {
	auto result = connection->execute(R""""(SELECT "repo" FROM "Packages" WHERE "id"=$1)"""", id);
	if (result.empty()) {
		return std::nullopt;
	} else {
		return result[0]["repo"].as<std::string>();
	}
}

std::optional<std::string> canister::db::current_vpackage_version(std::string package) {
	auto result = connection->execute(R""""(SELECT "version" FROM "VPackages" WHERE "package"=$1 AND "current_version"=true)"""", package);
	if (result.empty()) {
		return std::nullopt;
	} else {
		return result[0]["version"].as<std::string>();
	}
}

std::int8_t canister::db::repository_ranking(std::string slug) {
	auto result = connection->execute(R""""(SELECT "ranking" FROM "Repositories" WHERE slug=$1)"""", slug);
	if (result.empty()) {
		canister::log::error("db", "unexpectedly recieved no ranking for slug: " + slug);
		return 6; // This is higher than all rankings, causing it to be ignored where it's called
		// sentry_capture_event("") // TODO: Sentry & Also probably Throw
	} else {
		return result[0]["ranking"].as<std::int8_t>();
	}
}

void canister::db::set_current_vpackage(std::string uuid, std::string package) {
	try {
		connection->execute(R""""(UPDATE "VPackages" SET "current_version"=false WHERE "package"=$1 AND "current_version"=true)"""", package);
		connection->execute(R""""(UPDATE "VPackages" SET "current_version"=true WHERE "uuid"=$1 AND "package"=$2)"""", uuid, package);
	} catch (std::exception &exc) {
		canister::log::error("db", exc.what());
	}
}

void canister::db::write_package(canister::db::package data) {
	auto transaction = connection->transaction();
	auto statement = R""""(
		INSERT INTO "Packages" (
			id,
			repo,
			price
		) VALUES ($1, $2, $3)
		ON CONFLICT (id) DO UPDATE SET
			repo=$2,
			price=$3
	)"""";

	try {
		transaction->execute(statement, data.id, data.repo, data.price);
		transaction->commit();
		canister::log::info("db", "inserted_package: " + data.id);
	} catch (std::exception &exc) {
		canister::log::error("db", exc.what());
		transaction->rollback();
	}
}

void canister::db::write_vpackage(canister::db::vpackage data) {
	auto transaction = connection->transaction();
	auto statement = R""""(
		INSERT INTO "VPackages" (
			uuid,
			package,
			current_version,
			version,
			architecture,
			filename,
			sha_256,
			name,
			description,
			author,
			maintainer,
			depiction,
			native_depiction,
			header,
			tint_color,
			icon,
			section,
			tag,
			installed_size,
			size
		) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20)
		ON CONFLICT (uuid) DO UPDATE SET
			package=$2,
			current_version=$3,
			version=$4,
			architecture=$5,
			filename=$6,
			sha_256=$7,
			name=$8,
			description=$9,
			author=$10,
			maintainer=$11,
			depiction=$12,
			native_depiction=$13,
			header=$14,
			tint_color=$15,
			icon=$16,
			section=$17,
			tag=$18,
			installed_size=$19,
			size=$20
	)"""";

	try {
		transaction->execute(
			statement,
			data.uuid,
			data.package,
			data.current_version,
			data.version,
			data.architecture,
			data.filename,
			data.sha_256,
			data.name,
			data.description,
			data.author,
			data.maintainer,
			data.depiction,
			data.native_depiction,
			data.header,
			data.tint_color,
			data.icon,
			data.section,
			data.tag,
			data.installed_size,
			data.size);

		transaction->commit();
		canister::log::info("db", "inserted_vpackage: " + data.package + ":" + data.version);
	} catch (std::exception &exc) {
		canister::log::error("db", exc.what());
		transaction->rollback();
	}
}

void canister::db::write_repository(canister::db::repository data) {
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
		) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14)
		ON CONFLICT (slug) DO UPDATE SET
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
		transaction->execute(
			statement,
			data.slug,
			data.aliases,
			data.ranking,
			data.package_count,
			data.sections,
			data.uri,
			data.dist,
			data.suite,
			data.name,
			data.version,
			data.description,
			data.date,
			data.payment_gateway,
			data.sileo_endpoint);

		transaction->commit();
		canister::log::info("db", "inserted_release: " + data.slug);
	} catch (std::exception &exc) {
		canister::log::error("db", exc.what());
		transaction->rollback();
	}
}
