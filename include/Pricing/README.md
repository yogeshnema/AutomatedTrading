# Pricing domain

This tree owns financial instruments and asset-class pricing contracts. It must not contain numerical valuation algorithms.

- `AssetClasses/` contains products grouped by economic asset class.
- `Analytics/Pricing/` contains reusable models and numerical methods.
- `Services/Pricing/` orchestrates persistence, model selection, and requests.

The existing services directly under `Pricing/` are legacy application utilities. Move them into an appropriate asset-class, analytics, or service package when they are next changed.
