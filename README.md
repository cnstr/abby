# Canister Core
This is the core service of Canister (version 1).<br>
It is responsible for handling the following tasks:
* Indexing repositories
* Caching package files
* Updating the database
* Parsing package files

### Development
This project works best in Docker.<br>
If you run this locally please ensure you are on Ubuntu 22.04 Jammy Jellyfish and have installed the following runtime dependencies AND development dependencies.

**Runtime Dependencies**
```
sudo apt install \
	libcurl4 \
	libbz2-1.0 \
	liblzma5 \
	libpq5 \
	libzstd1 \
	zlib1g
```

**Development Dependencies**
```
sudo apt install \
	build-essential \
	cmake \
	meson \
	ninja-build \
	libcurl4-openssl-dev \
	libbz2-dev \
	liblzma-dev \
	libpq-dev \
	libzstd-dev \
	zlib1g-dev
```

> Unauthorized copying of the accompanying files, via any medium is strictly prohibited. The resources attached with this license are proprietary and confidential. Copyright (C) 2021 Aerum LLC
