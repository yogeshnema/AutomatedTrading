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
  createdAt: string
  updatedAt: string
}

export type TradeInput = Pick<Trade, 'name' | 'productType' | 'underlying' | 'currency' | 'notional' | 'status' | 'economics'> & {
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
  optionType: 'CE' | 'PE'
  subscribed: boolean
  lastRefreshAt?: string
  lastError?: string
}

export interface MarketSubscription extends Omit<MarketInstrument, 'name' | 'subscribed'> {
  interval: 'minute'
  enabled: boolean
  lastCandleAt?: string
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
