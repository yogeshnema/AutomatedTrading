#pragma once

#include "KiteSession.h"
#include "VolArbitrageModels.h"

#include <string>
#include <vector>

class KiteInstrumentService
{
public:
    explicit KiteInstrumentService(const KiteSession& session);

    std::string downloadInstrumentMaster() const;

    std::vector<OptionInstrument> getNiftyOptionInstruments() const;

    std::vector<OptionInstrument> getNiftyOptionInstrumentsWithDailyCache(
        const std::string& cacheDirectory) const;

    std::vector<OptionInstrument> loadNiftyOptionInstrumentsFromCsvFile(
        const std::string& filePath) const;

    std::vector<OptionInstrument> parseOptionInstrumentsFromCsv(
        const std::string& csvText,
        const std::string& underlyingName = "NIFTY",
        const std::string& exchange = "NFO") const;

private:
    const KiteSession& session_;
};
