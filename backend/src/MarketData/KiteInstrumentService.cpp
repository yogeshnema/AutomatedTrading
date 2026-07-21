#include "MarketData/KiteInstrumentService.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <cpr/cpr.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace
{
    std::string trim(const std::string& value)
    {
        const auto begin = std::find_if_not(value.begin(), value.end(),
            [](unsigned char character) { return std::isspace(character); });

        const auto end = std::find_if_not(value.rbegin(), value.rend(),
            [](unsigned char character) { return std::isspace(character); }).base();

        if (begin >= end) {
            return {};
        }

        return std::string(begin, end);
    }

    std::vector<std::string> parseCsvLine(const std::string& line)
    {
        std::vector<std::string> fields;
        std::string field;
        bool inQuotes = false;

        for (size_t index = 0; index < line.size(); ++index) {
            const char character = line[index];

            if (character == '"') {
                if (inQuotes && index + 1 < line.size() && line[index + 1] == '"') {
                    field += '"';
                    ++index;
                }
                else {
                    inQuotes = !inQuotes;
                }
            }
            else if (character == ',' && !inQuotes) {
                fields.push_back(trim(field));
                field.clear();
            }
            else {
                field += character;
            }
        }

        fields.push_back(trim(field));
        return fields;
    }

    std::unordered_map<std::string, size_t> buildHeaderIndex(const std::vector<std::string>& headers)
    {
        std::unordered_map<std::string, size_t> headerIndex;
        for (size_t index = 0; index < headers.size(); ++index) {
            headerIndex[headers[index]] = index;
        }

        return headerIndex;
    }

    std::string getField(
        const std::vector<std::string>& fields,
        const std::unordered_map<std::string, size_t>& headerIndex,
        const std::string& name)
    {
        const auto header = headerIndex.find(name);
        if (header == headerIndex.end() || header->second >= fields.size()) {
            return {};
        }

        return fields[header->second];
    }

    OptionType parseOptionType(const std::string& instrumentType)
    {
        if (instrumentType == "CE") {
            return OptionType::Call;
        }

        if (instrumentType == "PE") {
            return OptionType::Put;
        }

        throw std::invalid_argument("Unsupported option instrument type: " + instrumentType);
    }

    std::string todayText()
    {
        const std::time_t now = std::time(nullptr);
        std::tm localTime{};

#ifdef _WIN32
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif

        std::ostringstream date;
        date << localTime.tm_year + 1900 << "-"
            << std::setw(2) << std::setfill('0') << localTime.tm_mon + 1 << "-"
            << std::setw(2) << std::setfill('0') << localTime.tm_mday;

        return date.str();
    }

    void writeTextFile(const std::filesystem::path& filePath, const std::string& text)
    {
        std::ofstream output(filePath, std::ios::binary);
        if (!output) {
            throw std::runtime_error("Unable to write instruments cache file: " + filePath.string());
        }

        output << text;
    }
}

KiteInstrumentService::KiteInstrumentService(const KiteSession& session)
    : session_(session)
{
}

std::string KiteInstrumentService::downloadInstrumentMaster() const
{
    auto response = cpr::Get(
        cpr::Url{ "https://api.kite.trade/instruments" },
        session_.commonHeaders());

    if (response.status_code != 200) {
        throw std::runtime_error(
            "Kite instruments API error: HTTP " +
            std::to_string(response.status_code) +
            "\n" + response.text);
    }

    return response.text;
}

std::vector<OptionInstrument> KiteInstrumentService::getNiftyOptionInstruments() const
{
    return parseOptionInstrumentsFromCsv(downloadInstrumentMaster());
}

std::vector<OptionInstrument> KiteInstrumentService::getNiftyOptionInstrumentsWithDailyCache(
    const std::string& cacheDirectory) const
{
    const auto cachePath = std::filesystem::path(cacheDirectory);
    const auto datedFilePath = cachePath / ("kite_instruments_" + todayText() + ".csv");
    const auto latestFilePath = cachePath / "latest.csv";

    if (std::filesystem::exists(datedFilePath)) {
        std::cout << "[instrument-cache] loading " << datedFilePath.string() << "\n";
        return loadNiftyOptionInstrumentsFromCsvFile(datedFilePath.string());
    }

    std::cout << "[instrument-cache] no cache for today. Downloading from Kite.\n";
    const auto csvText = downloadInstrumentMaster();

    std::filesystem::create_directories(cachePath);
    writeTextFile(datedFilePath, csvText);
    writeTextFile(latestFilePath, csvText);

    std::cout << "[instrument-cache] saved " << datedFilePath.string() << "\n";
    std::cout << "[instrument-cache] updated " << latestFilePath.string() << "\n";

    return parseOptionInstrumentsFromCsv(csvText);
}

std::vector<OptionInstrument> KiteInstrumentService::loadNiftyOptionInstrumentsFromCsvFile(
    const std::string& filePath) const
{
    std::ifstream input(filePath);
    if (!input) {
        throw std::runtime_error("Unable to open instruments file: " + filePath);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parseOptionInstrumentsFromCsv(buffer.str());
}

std::vector<OptionInstrument> KiteInstrumentService::parseOptionInstrumentsFromCsv(
    const std::string& csvText,
    const std::string& underlyingName,
    const std::string& exchange) const
{
    std::istringstream stream(csvText);
    std::string line;

    if (!std::getline(stream, line)) {
        return {};
    }

    const auto headers = parseCsvLine(line);
    const auto headerIndex = buildHeaderIndex(headers);
    std::vector<OptionInstrument> instruments;

    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }

        const auto fields = parseCsvLine(line);
        const auto name = getField(fields, headerIndex, "name");
        const auto rowExchange = getField(fields, headerIndex, "exchange");
        const auto instrumentType = getField(fields, headerIndex, "instrument_type");

        if (name != underlyingName || rowExchange != exchange) {
            continue;
        }

        if (instrumentType != "CE" && instrumentType != "PE") {
            continue;
        }

        OptionInstrument instrument{};
        instrument.instrumentToken = std::stol(getField(fields, headerIndex, "instrument_token"));
        instrument.exchange = rowExchange;
        instrument.tradingSymbol = getField(fields, headerIndex, "tradingsymbol");
        instrument.name = name;
        instrument.expiry = getField(fields, headerIndex, "expiry");
        instrument.strike = std::stod(getField(fields, headerIndex, "strike"));
        const auto lotSize = getField(fields, headerIndex, "lot_size");
        instrument.lotSize = lotSize.empty() ? 1.0 : std::stod(lotSize);
        instrument.optionType = parseOptionType(instrumentType);

        instruments.push_back(instrument);
    }

    std::sort(instruments.begin(), instruments.end(),
        [](const OptionInstrument& lhs, const OptionInstrument& rhs) {
            if (lhs.expiry != rhs.expiry) {
                return lhs.expiry < rhs.expiry;
            }

            if (lhs.strike != rhs.strike) {
                return lhs.strike < rhs.strike;
            }

            return static_cast<int>(lhs.optionType) < static_cast<int>(rhs.optionType);
        });

    return instruments;
}
