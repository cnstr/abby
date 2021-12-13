#!/usr/bin/env bash
if [[ -d "lib" ]]; then
	exit 0
fi

command git submodule update --init --recursive
command rm -rf lib/ && mkdir -p lib/

WITH_BORINGSSL=1

command make clean -C vendor/uws/uSockets
command make -j8 -C vendor/uws/uSockets

command cp vendor/uws/uSockets/uSockets.a lib/uws.a
command make clean -C vendor/uws/uSockets

command cmake -Blib/validator -Svendor/validator -DHUNTER_ENABLED=ON
command make -j8 -C lib/validator

command cp lib/validator/libnlohmann_json_schema_validator.a lib/validator.a
command rm -rf lib/validator

command cmake -Blib/curl -Svendor/curl
command make -j8 -C lib/curl

command cp lib/curl/libcurlpp.a lib/curlpp.a
command rm -rf lib/curl

# Pending: https://github.com/getsentry/sentry-native/issues/501
command cd vendor/sentry/external/breakpad && git checkout handler && cd -
command cmake -Blib/sentry -Svendor/sentry -DCMAKE_BUILD_TYPE=RelWithDebInfo
command make -j8 -C lib/sentry

command cp lib/sentry/libsentry.so lib/libsentry.so
command rm -rf lib/sentry

command cmake -Blib/pq -Svendor/pq -DPostgreSQL_TYPE_INCLUDE_DIR=/usr/include/postgresql/
command make -j8 -C lib/pq

command cp lib/pq/libtaopq.a lib/libtaopq.a
command rm -rf lib/pq
