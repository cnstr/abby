CREATE TABLE IF NOT EXISTS "Repositories" (
	"slug" VARCHAR(255) NOT NULL PRIMARY KEY,
	"aliases" VARCHAR(255)[] NOT NULL,
	"ranking" SMALLINT NOT NULL,
	"package_count" INT NOT NULL,
	"sections" VARCHAR(255)[] NOT NULL,

	"uri" TEXT NOT NULL,
	"dist" VARCHAR(255),
	"suite" VARCHAR(255),

	"name" TEXT,
	"version" TEXT,
	"description" TEXT,
	"date" TEXT,
	"payment_gateway" TEXT,
	"sileo_endpoint" TEXT
);
---
CREATE TABLE IF NOT EXISTS "Packages" (
	"id" VARCHAR(255) NOT NULL PRIMARY KEY,
	"repo" VARCHAR(255) NOT NULL,
	"price" REAL NOT NULL,

	FOREIGN KEY("repo") REFERENCES "Repositories"("slug")
);
---
CREATE TABLE IF NOT EXISTS "VPackages" (
	"uuid" VARCHAR(255) NOT NULL PRIMARY KEY,
	"package" VARCHAR(255) NOT NULL REFERENCES "Packages",
	"current_version" BOOLEAN NOT NULL DEFAULT false,

	"version" VARCHAR(255) NOT NULL,
	"architecture" VARCHAR(255) NOT NULL,
	"filename" TEXT NOT NULL,

	"sha_256" TEXT,
	"name" TEXT,
	"description" TEXT,
	"author" TEXT,
	"maintainer" TEXT,
	"depiction" TEXT,
	"native_depiction" TEXT,
	"header" TEXT,
	"tint_color" TEXT,
	"icon" TEXT,
	"section" TEXT,
	"tag" TEXT,
	"installed_size" TEXT,
	"size" TEXT
);
---
CREATE UNIQUE INDEX IF NOT EXISTS "SingularCurrentPackage" ON "VPackages"(package) WHERE "current_version";
