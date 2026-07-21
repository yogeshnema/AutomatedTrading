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
