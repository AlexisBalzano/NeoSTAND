#pragma once
#include <array>
#include <optional>

namespace stand {
    using Color = std::optional<std::array<unsigned int, 3>>;
    enum class ColorName {
        UNCONFIRMED = 0,
        CONFIRMED
    };
}