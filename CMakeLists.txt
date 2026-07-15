cmake_minimum_required(VERSION 3.20)

project(AutomatedTrading LANGUAGES CXX)

find_package(cpr CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cppzmq CONFIG REQUIRED)

add_executable(AutomatedTrading
    src/main.cpp
    src/MarketData/IMarketDataProvider.cpp
    src/MarketData/KiteInstrumentService.cpp
    src/MarketData/KiteMarketDataProvider.cpp
    src/MarketData/KiteMarketDataService.cpp
    src/MarketData/MarketDataEvent.cpp
    src/MarketData/OptionChainService.cpp
    src/MarketData/PolygonMarketDataProvider.cpp
    src/MarketData/PublishingMarketDataProvider.cpp
    src/MarketData/QuoteService.cpp
    src/Networking/EventMessaging.cpp
    src/Networking/KiteSession.cpp
    src/Networking/MarketDataRecorder.cpp
    src/Pricing/ForwardPriceService.cpp
    src/Pricing/ImpliedVolService.cpp
    src/Pricing/VolSurfaceService.cpp
    src/Risk/BacktestService.cpp
    src/Risk/SignalService.cpp
)

target_compile_features(AutomatedTrading PRIVATE cxx_std_20)

target_include_directories(AutomatedTrading
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(AutomatedTrading
    PRIVATE
        cpr::cpr
        nlohmann_json::nlohmann_json
        cppzmq
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(AutomatedTrading PRIVATE -Wall -Wextra -Wpedantic)
endif()
