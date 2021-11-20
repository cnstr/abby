FROM ubuntu:jammy AS build
WORKDIR /build

RUN apt-get update \
	&& DEBIAN_FRONTEND=noninteractive apt-get install -yq \
		build-essential \
		cmake \
		meson \
		ninja-build \
		libcurl4-openssl-dev \
		libbz2-dev \
		liblzma-dev \
		libzstd-dev \
	&& rm -rf /var/lib/apt/lists/*

COPY . .
RUN meson setup staging \
	&& cd staging \
	&& meson compile

FROM ubuntu:jammy
RUN apt-get update \
	&& DEBIAN_FRONTEND=noninteractive apt-get install -yq \
		libcurl4 \
		libbz2-1.0 \
		liblzma5 \
		libzstd1 \
	&& rm -rf /var/lib/apt/lists/*

COPY --from=build /build/staging/core ./
CMD ["./core"]
