#include <canister.h>

int canister::dpkg::compare(const std::string &left_raw, const std::string &right_raw) {
	canister::dpkg::version left, right;
	std::string::size_type index;

	// Find out if we have an epoch (dpkg defaults to 0 if it doesn't exist)
	index = left_raw.find(":", 0);
	if (index == std::string::npos) {
		left.epoch = 0;
		left.version = left_raw;
	} else {
		left.epoch = std::stoi(left_raw.substr(0, index));
		left.version = left_raw.substr(index + 1, left_raw.length());
	}

	index = right_raw.find(":", 0);
	if (index == std::string::npos) {
		right.epoch = 0;
		right.version = right_raw;
	} else {
		right.epoch = std::stoi(right_raw.substr(0, index));
		right.version = right_raw.substr(index + 1, right_raw.length());
	}

	// Find out the version strings and their revisions if applicable
	index = left.version.rfind("-", left_raw.length());
	if (index == std::string::npos) {
		left.revision = "0";
	} else {
		left.revision = left.version.substr(index + 1, left.version.length());
		left.version = left.version.substr(0, index);
	}

	index = right.version.rfind("-", right_raw.length());
	if (index == std::string::npos) {
		right.revision = "0";
	} else {
		right.revision = right.version.substr(index + 1, right.version.length());
		right.version = right.version.substr(0, index);
	}

	// Compare everything
	int return_code;
	if (left.epoch > right.epoch) {
		return 1;
	}

	if (left.epoch < right.epoch) {
		return -1;
	}

	if (int status = canister::dpkg::verrevcmp(left.version.c_str(), right.version.c_str())) {
		return status;
	}

	return verrevcmp(left.revision.c_str(), right.revision.c_str());
}

// These are taken from dpkg with minor modifications to build with C++20 and Canister
// https://git.dpkg.org/cgit/dpkg/dpkg.git/tree/lib/dpkg/version.c
static int canister::dpkg::order(int c) {
	if (isdigit(c))
		return 0;
	else if (isalpha(c))
		return c;
	else if (c == '~')
		return -1;
	else if (c)
		return c + 256;
	else
		return 0;
}

static int canister::dpkg::verrevcmp(const char *a, const char *b) {
	if (a == NULL)
		a = "";
	if (b == NULL)
		b = "";

	while (*a || *b) {
		int first_diff = 0;

		while ((*a && !isdigit(*a)) || (*b && !isdigit(*b))) {
			int ac = order(*a);
			int bc = order(*b);

			if (ac != bc)
				return ac - bc;

			a++;
			b++;
		}
		while (*a == '0')
			a++;
		while (*b == '0')
			b++;
		while (isdigit(*a) && isdigit(*b)) {
			if (!first_diff)
				first_diff = *a - *b;
			a++;
			b++;
		}

		if (isdigit(*a))
			return 1;
		if (isdigit(*b))
			return -1;
		if (first_diff)
			return first_diff;
	}

	return 0;
}
