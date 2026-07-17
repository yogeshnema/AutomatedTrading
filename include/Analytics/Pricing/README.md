# Pricing analytics

This tree owns asset-independent pricing interfaces, valuation results, models, and numerical methods. Market data must evolve into generic typed inputs; equity-only data should move out of shared `PricingTypes` during implementation.

Models describe financial dynamics or closed-form assumptions. Numerical methods solve or simulate those models. Neither layer owns trade persistence or HTTP orchestration.
