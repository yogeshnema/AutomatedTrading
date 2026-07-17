# Legacy Black-Scholes location

This folder contains the currently compiled Black-Scholes pricer. Its planned destination is `Analytics/Pricing/Models/BlackScholes`.

Keep compatibility here until includes, build files, tests, and downstream callers can be migrated together. New shared numerical machinery should be added under `NumericalMethods`, not in this legacy folder.
