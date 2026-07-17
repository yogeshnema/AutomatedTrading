#pragma once

#include <cstdlib>
#include <optional>
#include <string>

namespace automated_trading::common
{
    inline std::optional<std::string> environmentValue(const char* name)
    {
#ifdef _WIN32
        char* buffer = nullptr;
        std::size_t length = 0;
        const errno_t result = _dupenv_s(&buffer, &length, name);

        if (result != 0 || buffer == nullptr) {
            std::free(buffer);
            return std::nullopt;
        }

        std::string value(buffer);
        std::free(buffer);
#else
        const char* rawValue = std::getenv(name);
        if (rawValue == nullptr) {
            return std::nullopt;
        }

        std::string value(rawValue);
#endif

        if (value.empty()) {
            return std::nullopt;
        }
        return value;
    }
}
