cmake_minimum_required(VERSION 3.20)

project(AutomatedTrading LANGUAGES CXX)

find_package(cpr CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cppzmq CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(httplib CONFIG QUIET)
if(NOT httplib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        cpp_httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.28.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(cpp_httplib)
endif()

add_executable(AutomatedTrading
    src/main.cpp
    src/Analytics/Pricing/BlackScholes/BlackScholesPricer.cpp
    src/Analytics/Pricing/PricingTypes.cpp
    src/Database/DatabaseTypes.cpp
    src/Database/InsertService.cpp
    src/Database/PostgresConnection.cpp
    src/Database/QueryService.cpp
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
    src/Products/Equity/Options/European/EuropeanVanillaOption.cpp
    src/Products/Equity/Options/Exercise/EuropeanExercise.cpp
    src/Products/Equity/Options/Payoffs/VanillaPayoff.cpp
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
        PostgreSQL::PostgreSQL
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(AutomatedTrading PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(BlackScholesPricerTests
        tests/BlackScholesPricerTests.cpp
        src/Analytics/Pricing/BlackScholes/BlackScholesPricer.cpp
        src/Analytics/Pricing/PricingTypes.cpp
        src/Products/Equity/Options/European/EuropeanVanillaOption.cpp
        src/Products/Equity/Options/Exercise/EuropeanExercise.cpp
        src/Products/Equity/Options/Payoffs/VanillaPayoff.cpp
    )
    target_compile_features(BlackScholesPricerTests PRIVATE cxx_std_20)
    target_include_directories(BlackScholesPricerTests PRIVATE ${PROJECT_SOURCE_DIR}/include)
    add_test(NAME BlackScholesPricerTests COMMAND BlackScholesPricerTests)

    add_executable(PricingApplicationServiceTests
        tests/PricingApplicationServiceTests.cpp
        src/Services/Pricing/PricingApplicationService.cpp
        src/Services/Pricing/PostgresTradePricingRepository.cpp
        src/Analytics/Pricing/BlackScholes/BlackScholesPricer.cpp
        src/Analytics/Pricing/PricingTypes.cpp
        src/Database/DatabaseTypes.cpp
        src/Database/QueryService.cpp
        src/Products/Equity/Options/European/EuropeanVanillaOption.cpp
        src/Products/Equity/Options/Exercise/EuropeanExercise.cpp
        src/Products/Equity/Options/Payoffs/VanillaPayoff.cpp
    )
    target_compile_features(PricingApplicationServiceTests PRIVATE cxx_std_20)
    target_include_directories(PricingApplicationServiceTests PRIVATE ${PROJECT_SOURCE_DIR}/include)
    add_test(NAME PricingApplicationServiceTests COMMAND PricingApplicationServiceTests)
endif()

add_executable(PricingService
    src/Services/Pricing/main.cpp
    src/Services/Pricing/PostgresTradePricingRepository.cpp
    src/Services/Pricing/PricingApplicationService.cpp
    src/Analytics/Pricing/BlackScholes/BlackScholesPricer.cpp
    src/Analytics/Pricing/PricingTypes.cpp
    src/Database/DatabaseTypes.cpp
    src/Database/PostgresConnection.cpp
    src/Database/QueryService.cpp
    src/Products/Equity/Options/European/EuropeanVanillaOption.cpp
    src/Products/Equity/Options/Exercise/EuropeanExercise.cpp
    src/Products/Equity/Options/Payoffs/VanillaPayoff.cpp
)
target_compile_features(PricingService PRIVATE cxx_std_20)
target_include_directories(PricingService PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(PricingService
    PRIVATE
        httplib::httplib
        nlohmann_json::nlohmann_json
        PostgreSQL::PostgreSQL
)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(PricingService PRIVATE -Wall -Wextra -Wpedantic)
endif()
