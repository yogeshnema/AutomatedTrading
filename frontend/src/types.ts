export type TradeStatus = 'DRAFT' | 'ACTIVE' | 'MATURED' | 'CANCELLED'

export interface Trade {
  id: string
  version: number
  name: string
  productType: string
  underlying: string
  currency: string
  notional: number
  status: TradeStatus
  economics: Record<string, unknown>
  instrumentToken?: number | null
  tradingSymbol?: string | null
  expiry?: string | null
  strike?: number | null
  optionType?: 'CE' | 'PE' | null
  lotSize?: number | null
  side?: 'buy' | 'sell' | null
  quantity?: number | null
  executionPrice?: number | null
  spotSeriesCode?: string | null
  impliedVolSeriesCode?: string | null
  rateSeriesCode?: string | null
  createdAt: string
  updatedAt: string
}

export interface TradeInput {
  name: string
  status: TradeStatus
  instrumentToken: number
  side: 'buy' | 'sell'
  quantity: number
  executionPrice: number
  impliedVolSeriesCode: string
  rateSeriesCode: string
  version?: number
}

export interface ServiceState {
  id: 'trade-library' | 'pricing' | 'risk' | 'market-data' | 'stream'
  name: string
  endpoint: string
  status: 'ready' | 'offline' | 'checking'
  latency?: number
}

export interface MarketInstrument {
  instrumentToken: number
  exchange: string
  tradingSymbol: string
  name: string
  expiry: string
  strike: number
  lotSize?: number
  optionType: 'CE' | 'PE'
  subscribed: boolean
  lastRefreshAt?: string
  lastError?: string
}

export interface MarketSubscription extends Omit<MarketInstrument, 'name' | 'subscribed' | 'lotSize'> {
  interval: 'minute' | 'day'
  enabled: boolean
  lastCandleAt?: string
}

export interface InstrumentFacets {
  names: string[]
  expiries: string[]
  strikes: number[]
  optionTypes: Array<'CE' | 'PE'>
}

export interface ReferenceSeries {
  seriesCode: string
  seriesType: 'underlying_spot' | 'implied_volatility' | 'risk_free_rate'
  displayName: string
  provider: string
  currency?: string | null
  tenor?: string | null
  acquisitionMode: 'vendor' | 'derived' | 'manual'
  description?: string | null
  subscribed: boolean
  status?: string | null
  lastObservedAt?: string | null
  lastError?: string | null
}

export interface MarketDataStatus {
  databaseReady: boolean
  workerRunning: boolean
  instrumentCount: number
  activeSubscriptions: number
  instrumentMasterRefreshedAt?: string
  pollSeconds: number
}

export interface MarketCandle {
  timestamp: string
  open: number
  high: number
  low: number
  close: number
  volume: number
}

export interface MarketDataSnapshot {
  instrument: MarketInstrument
  minuteCandles: MarketCandle[]
  eod?: MarketCandle | null
  summary: {
    date: string
    open?: number | null
    high?: number | null
    low?: number | null
    close?: number | null
    absoluteChange?: number | null
    percentChange?: number | null
    volume: number
    minuteCandleCount: number
    firstCandleAt?: string | null
    lastCandleAt?: string | null
    eodPersisted: boolean
  }
}
