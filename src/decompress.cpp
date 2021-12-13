#include <canister.h>

void canister::decompress::gz(const std::string id, const std::string archive, const std::string cache) {
	std::ifstream archive_file(archive.c_str());
	if (!archive_file.is_open()) {
		throw std::runtime_error(id + " - gz: failed to open archive at " + archive);
	}

	// Converts our ifstream to a string using streambuf iterators
	auto iterator = std::istreambuf_iterator<char>(archive_file);
	std::string buffer_data = std::string(iterator, std::istreambuf_iterator<char>());
	std::string output_data;
	size_t max_size(10000000);
	z_stream inflate_stream;

	// Zlib why do you feel a need
	inflate_stream.zalloc = Z_NULL;
	inflate_stream.zfree = Z_NULL;
	inflate_stream.opaque = Z_NULL;
	inflate_stream.next_in = Z_NULL;
	inflate_stream.avail_in = 0;

	// windowBits 15
	// ENABLE_ZLIB_GZIP 32
	int status = inflateInit2(&inflate_stream, 15 | 32);
	if (status < 0) {
		throw std::runtime_error(id + " - gz: invalid zlib handle");
	}

	std::size_t inflated_size = 0;
	std::size_t buffer_size = buffer_data.size();

	// Because of Zlib's weird pointer magic, we need to reinterpret this as a pointer first
	inflate_stream.next_in = reinterpret_cast<z_const Bytef *>(buffer_data.data());
	if (buffer_size > max_size || (buffer_size * 2) > max_size) {
		inflateEnd(&inflate_stream);
		throw std::runtime_error(id + " - gz: inflate is using too much memory");
	}

	inflate_stream.avail_in = static_cast<unsigned int>(buffer_size);

	do {
		// The string buffer needs a reference of it's next resize so it may expand to fit data
		std::size_t next_resize = inflated_size + 2 * buffer_size;

		if (next_resize > max_size) {
			inflateEnd(&inflate_stream);
			throw std::runtime_error(id + " - gz: inflated size exceeds maximum size");
		}

		output_data.resize(next_resize);

		// Expand the size of our Zlib stream too so it can continue inflating
		inflate_stream.avail_out = static_cast<unsigned int>(2 * buffer_size);
		inflate_stream.next_out = reinterpret_cast<Bytef *>(&output_data[0] + inflated_size);

		int inflate_status = inflate(&inflate_stream, Z_FINISH);

		// If this happens we didn't reach Z_FINISH and the buffer is just dead
		if (inflate_status != Z_STREAM_END && inflate_status != Z_OK && inflate_status != Z_BUF_ERROR) {
			std::string error_message = inflate_stream.msg;
			inflateEnd(&inflate_stream);

			throw std::runtime_error(id + " - gz: decompression error > " + error_message);
		}

		inflated_size += (2 * buffer_size - inflate_stream.avail_out);
	} while (inflate_stream.avail_out == 0);

	inflateEnd(&inflate_stream);
	output_data.resize(inflated_size);

	// Write the data to a file
	std::ofstream cache_file(cache.c_str());
	if (!cache_file.is_open()) {
		throw std::runtime_error(id + " - gz: failed to make extract handle at " + cache);
	}

	cache_file << output_data;
}

void canister::decompress::xz(const std::string id, const std::string archive, const std::string cache) {
	FILE *const file_in = fopen(archive.c_str(), "rb");
	FILE *const file_out = fopen(cache.c_str(), "wb+");

	if (!file_in) {
		fclose(file_out);
		throw std::runtime_error(id + " - xz: failed to open archive at " + archive);
	}

	if (!file_out) {
		fclose(file_in);
		throw std::runtime_error(id + " - xz: failed to make extract handle at " + cache);
	}

	lzma_stream macro_stream = LZMA_STREAM_INIT;
	lzma_ret status = lzma_stream_decoder(&macro_stream, UINT64_MAX, LZMA_CONCATENATED);
	lzma_stream *stream = reinterpret_cast<lzma_stream *>(&macro_stream);

	switch (status) {
		case LZMA_OK:
			break;

		case LZMA_MEM_ERROR:
			lzma_end(stream);
			throw std::runtime_error(id + " - xz: stream memory error");

		case LZMA_OPTIONS_ERROR:
			lzma_end(stream);
			throw std::runtime_error(id + " - xz: stream options error");

		default:
			lzma_end(stream);
			throw std::runtime_error(id + " - xz: stream init error");
	}

	lzma_action action = LZMA_RUN;
	uint8_t in_buf[BUFSIZ];
	uint8_t out_buf[BUFSIZ];

	stream->next_in = NULL;
	stream->avail_in = 0;
	stream->next_out = out_buf;
	stream->avail_out = sizeof(out_buf);

	for (;;) { // Break inside when finished
		if (stream->avail_in == 0 && !feof(file_in)) {
			stream->next_in = in_buf;
			stream->avail_in = fread(in_buf, 1, sizeof(in_buf), file_in);

			if (ferror(file_in)) {
				throw std::runtime_error(id + " - xz: read error > " + strerror(errno));
			}

			if (feof(file_in)) {
				action = LZMA_FINISH;
			}
		}

		lzma_ret status = lzma_code(stream, action);
		if (stream->avail_out == 0 || status == LZMA_STREAM_END) {
			size_t write_size = sizeof(out_buf) - stream->avail_out;

			if (std::fwrite(out_buf, 1, write_size, file_out) != write_size) {
				throw std::runtime_error(id + " - xz: write error > " + strerror(errno));
			}

			stream->next_out = out_buf;
			stream->avail_out = sizeof(out_buf);
		}

		if (status != LZMA_OK) {
			switch (status) {
				case LZMA_STREAM_END:
					lzma_end(stream);
					return;

				case LZMA_MEM_ERROR:
					lzma_end(stream);
					throw std::runtime_error(id + " - xz: stream memory error");

				case LZMA_OPTIONS_ERROR:
					lzma_end(stream);
					throw std::runtime_error(id + " - xz: stream options error");

				default:
					lzma_end(stream);
					throw std::runtime_error(id + " - xz: stream decompression error");
			}
		}
	}

	fclose(file_in);
	fclose(file_out);
}

void canister::decompress::bz2(const std::string id, const std::string archive, const std::string cache) {
	char output_buffer[BUFSIZ];
	FILE *const file_in = fopen(archive.c_str(), "rb");
	FILE *const file_out = fopen(cache.c_str(), "wb+");

	if (!file_in) {
		fclose(file_out);
		throw std::runtime_error(id + " - bz2: failed to open archive at " + archive);
	}

	if (!file_out) {
		fclose(file_in);
		throw std::runtime_error(id + " - bz2: failed to make extract handle at " + cache);
	}

	int status = 0;
	BZFILE *bzip_handle = BZ2_bzReadOpen(&status, file_in, 0, 0, NULL, 0);

	if (status != BZ_OK) {
		BZ2_bzReadClose(&status, bzip_handle);
		fclose(file_out);
		fclose(file_in);

		throw std::runtime_error(id + " - bz2: invalid bzfile handle");
	}

	do {
		int read_position = BZ2_bzRead(&status, bzip_handle, output_buffer, sizeof output_buffer);
		if (status == BZ_OK || status == BZ_STREAM_END) {
			fwrite(output_buffer, 1, read_position, file_out);
		}
	} while (status == BZ_OK);

	if (status != BZ_STREAM_END) {
		BZ2_bzReadClose(&status, &bzip_handle);
		fclose(file_out);
		fclose(file_in);

		throw std::runtime_error(id + " - bz2: decompression error > " + BZ2_bzerror(&bzip_handle, &status));
	}

	fclose(file_out);
	fclose(file_in);

	std::ifstream cache_file(cache);
	if (!cache_file.is_open()) {
		throw std::runtime_error(id + " - bz2: failed to open decompressed file");
	}

	// Converts our ifstream to a string using streambuf iterators
	auto iterator = std::istreambuf_iterator<char>(cache_file);
	std::string cache_data = std::string(iterator, std::istreambuf_iterator<char>());
}

void canister::decompress::lzma(const std::string id, const std::string archive, const std::string cache) {
	FILE *const file_in = fopen(archive.c_str(), "rb");
	FILE *const file_out = fopen(cache.c_str(), "wb+");

	if (!file_in) {
		fclose(file_out);
		throw std::runtime_error(id + " - lzma: failed to open archive at " + archive);
	}

	if (!file_out) {
		fclose(file_in);
		throw std::runtime_error(id + " - lzma: failed to make extract handle at " + cache);
	}

	lzma_stream macro_stream = LZMA_STREAM_INIT;
	lzma_ret status = lzma_alone_decoder(&macro_stream, UINT64_MAX);
	lzma_stream *stream = reinterpret_cast<lzma_stream *>(&macro_stream);

	switch (status) {
		case LZMA_OK:
			break;

		case LZMA_MEM_ERROR:
			lzma_end(stream);
			throw std::runtime_error(id + " - lzma: stream memory error");

		default:
			lzma_end(stream);
			throw std::runtime_error(id + " - lzma: stream init error");
	}

	lzma_action action = LZMA_RUN;
	uint8_t in_buf[BUFSIZ];
	uint8_t out_buf[BUFSIZ];

	stream->next_in = NULL;
	stream->avail_in = 0;
	stream->next_out = out_buf;
	stream->avail_out = sizeof(out_buf);

	for (;;) { // Break inside when finished
		if (stream->avail_in == 0 && !feof(file_in)) {
			stream->next_in = in_buf;
			stream->avail_in = fread(in_buf, 1, sizeof(in_buf), file_in);

			if (ferror(file_in)) {
				throw std::runtime_error(id + " - lzma: read error > " + strerror(errno));
			}

			if (feof(file_in)) {
				action = LZMA_FINISH;
			}
		}

		lzma_ret status = lzma_code(stream, action);
		if (stream->avail_out == 0 || status == LZMA_STREAM_END) {
			size_t write_size = sizeof(out_buf) - stream->avail_out;

			if (std::fwrite(out_buf, 1, write_size, file_out) != write_size) {
				throw std::runtime_error(id + " - lzma: write error > " + strerror(errno));
			}

			stream->next_out = out_buf;
			stream->avail_out = sizeof(out_buf);
		}

		if (status != LZMA_OK) {
			switch (status) {
				case LZMA_STREAM_END:
					lzma_end(stream);
					return;

				case LZMA_MEM_ERROR:
					lzma_end(stream);
					throw std::runtime_error(id + " - lzma: stream memory error");

				case LZMA_OPTIONS_ERROR:
					lzma_end(stream);
					throw std::runtime_error(id + " - lzma: stream options error");

				default:
					lzma_end(stream);
					throw std::runtime_error(id + " - lzma: stream decompression error");
			}
		}
	}

	fclose(file_in);
	fclose(file_out);
}

void canister::decompress::zstd(const std::string id, const std::string archive, const std::string cache) {
	FILE *const file_in = fopen(archive.c_str(), "rb");
	FILE *const file_out = fopen(cache.c_str(), "wb+");

	if (!file_in) {
		fclose(file_out);
		throw std::runtime_error(id + " - zstd: failed to open archive at " + archive);
	}

	if (!file_out) {
		fclose(file_in);
		throw std::runtime_error(id + " - zstd: failed to make extract handle at " + cache);
	}

	// Streams don't require a set size, so we use ZSTD's allocated size
	size_t const buffer_in_size = ZSTD_DStreamInSize();
	size_t const buffer_out_size = ZSTD_DStreamOutSize();

	void *const buffer_in = malloc(buffer_in_size);
	void *const buffer_out = malloc(buffer_out_size);

	ZSTD_DCtx *const dict_ctx = ZSTD_createDCtx();
	if (dict_ctx == NULL) {
		fclose(file_in);
		free(buffer_in);
		free(buffer_out);

		throw std::runtime_error(id + " - zstd: invalid decompression context");
	}

	size_t read_size;
	size_t last_status = 0;
	size_t const pending_read = buffer_in_size;

	// We have to basically read every byte at a time and decompress/write it to a buffer
	while ((read_size = fread(buffer_in, 1, pending_read, file_in))) {
		ZSTD_inBuffer input = {
			buffer_in,
			read_size,
			0
		};

		while (input.pos < input.size) {
			ZSTD_outBuffer output = {
				buffer_out,
				buffer_out_size,
				0
			};

			size_t const status = ZSTD_decompressStream(dict_ctx, &output, &input);
			if (ZSTD_isError(status)) {
				ZSTD_freeDCtx(dict_ctx);
				fclose(file_in);
				fclose(file_out);
				free(buffer_in);
				free(buffer_out);

				throw std::runtime_error(id + " - zstd: decompression error > " + ZSTD_getErrorName(status));
			}

			fwrite(buffer_out, 1, output.pos, file_out);
			last_status = status;
		}
	}

	// If the last status was not okay that means decompression had an EOF
	if (last_status != 0) {
		ZSTD_freeDCtx(dict_ctx);
		fclose(file_in);
		fclose(file_out);
		free(buffer_in);
		free(buffer_out);

		throw std::runtime_error(id + " - zstd: unexpected EOF on last_status > " + ZSTD_getErrorName(last_status));
	}

	ZSTD_freeDCtx(dict_ctx);
	fclose(file_in);
	fclose(file_out);
	free(buffer_in);
	free(buffer_out);

	std::ifstream cache_file(cache);
	if (!cache_file.is_open()) {
		throw std::runtime_error(id + " - zstd: failed to open decompressed file");
	}

	// Converts our ifstream to a string using streambuf iterators
	auto iterator = std::istreambuf_iterator<char>(cache_file);
	std::string cache_data = std::string(iterator, std::istreambuf_iterator<char>());
}
