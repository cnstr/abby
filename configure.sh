#!/usr/bin/env bash
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
