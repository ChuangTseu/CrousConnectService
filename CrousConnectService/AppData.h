#pragma once

#include <string>
#include <vector>

namespace KnownGateways {
	namespace Crous {
		enum Enum { CROUS1 = 0, CROUS2, CROUS3, NB_CROUS_GATEWAYS };

		extern std::vector<std::string> const Ips;
		extern std::vector<std::string> const Names;
	}
}


