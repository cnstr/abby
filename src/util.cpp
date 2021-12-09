#include <canister.h>

std::string canister::util::timestamp() {
	const auto now = std::chrono::system_clock::now();
	return std::to_string(now.time_since_epoch().count());
}

std::string canister::util::cache_path() {
	return std::filesystem::temp_directory_path().string() + "/canister/";
}

std::string canister::util::safe_fs_name(const std::string token) {
	std::string value = token.substr(token.find("://") + 3);
	std::transform(value.begin(), value.end(), value.begin(), [](char value) {
		if (value == '.' || value == '/') {
			return '_';
		}

		return value;
	});

	return value;
}

bool canister::util::matched_hash(const std::string left, const std::string right) {
	std::vector<unsigned char> left_vector(picosha2::k_digest_size);
	picosha2::hash256(left.begin(), left.end(), left_vector.begin(), left_vector.end());
	std::string left_hash = picosha2::bytes_to_hex_string(left_vector.begin(), left_vector.end());

	std::vector<unsigned char> right_vector(picosha2::k_digest_size);
	picosha2::hash256(right.begin(), right.end(), right_vector.begin(), right_vector.end());
	std::string right_hash = picosha2::bytes_to_hex_string(right_vector.begin(), right_vector.end());

	return left_hash == right_hash;
}
