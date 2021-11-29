#include <canister.h>

void canister::decompress::bz2(const std::string id, const std::string archive, const std::string cache) {
	char output_buffer[4096];
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
	std::remove(archive.c_str()); // Remove the archive path when completed
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
	std::remove(archive.c_str()); // Remove the archive path when completed
}
