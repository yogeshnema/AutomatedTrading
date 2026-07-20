#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <utility>

namespace automated_trading::common
{
    inline std::optional<std::string> environmentValue(const char* name)
    {
#if defined(_MSC_VER)
        char* value = nullptr;
        std::size_t size = 0;
        if (_dupenv_s(&value, &size, name) != 0 || value == nullptr || size == 0) {
            if (value != nullptr) {
                std::free(value);
            }
            return std::nullopt;
        }

        std::string result(value);
        std::free(value);
        return result.empty() ? std::nullopt : std::optional<std::string>(std::move(result));
#else
        const char* value = std::getenv(name);
        if (value == nullptr || *value == '\0') {
            return std::nullopt;
        }
        return std::string(value);
#endif
    }
}
