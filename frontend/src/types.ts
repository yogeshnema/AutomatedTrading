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
  id: 'trade-library' | 'pricing' | 'risk' | 'stream'
  name: string
  endpoint: string
  status: 'ready' | 'offline' | 'checking'
  latency?: number
}
