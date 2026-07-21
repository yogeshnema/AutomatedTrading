import type { InstrumentFacets, MarketDataSnapshot, MarketDataStatus, MarketInstrument, MarketSubscription, ReferenceSeries, Trade, TradeInput } from '../types'

export const endpoints = {
  trades: import.meta.env.VITE_TRADE_LIBRARY_URL ?? '/trade-library',
  pricing: import.meta.env.VITE_PRICING_URL ?? '/pricing',
  risk: import.meta.env.VITE_RISK_URL ?? '/risk',
  marketData: import.meta.env.VITE_MARKET_DATA_URL ?? '/market-data',
  stream: import.meta.env.VITE_STREAM_URL ?? `${window.location.protocol === 'https:' ? 'wss:' : 'ws:'}//${window.location.host}/stream`,
}

export const marketDataApi = {
  status() {
    return request<MarketDataStatus>(`${endpoints.marketData}/api/v1/market-data/status`)
  },
  async instruments(filters: { search?: string; expiry?: string; optionType?: string; strike?: number } = {}) {
    const query = new URLSearchParams({ limit: '250' })
    if (filters.search) query.set('search', filters.search)
    if (filters.expiry) query.set('expiry', filters.expiry)
    if (filters.optionType) query.set('optionType', filters.optionType)
    if (filters.strike != null) query.set('strike', String(filters.strike))
    return (await request<{ items: MarketInstrument[] }>(`${endpoints.marketData}/api/v1/instruments?${query}`)).items
  },
  async subscriptions() {
    return (await request<{ items: MarketSubscription[] }>(`${endpoints.marketData}/api/v1/subscriptions`)).items
  },
  facets(filters: { name?: string; expiry?: string; optionType?: string } = {}) {
    const query = new URLSearchParams()
    if (filters.name) query.set('name', filters.name)
    if (filters.expiry) query.set('expiry', filters.expiry)
    if (filters.optionType) query.set('optionType', filters.optionType)
    return request<InstrumentFacets>(`${endpoints.marketData}/api/v1/instrument-facets?${query}`)
  },
  subscribe(instrumentToken: number, interval: 'minute' | 'day' = 'minute') {
    return request<MarketSubscription>(`${endpoints.marketData}/api/v1/subscriptions`, {
      method: 'POST', body: JSON.stringify({ instrumentToken, interval }),
    })
  },
  async referenceSeries(type?: ReferenceSeries['seriesType']) {
    const query = type ? `?type=${encodeURIComponent(type)}` : ''
    return (await request<{ items: ReferenceSeries[] }>(`${endpoints.marketData}/api/v1/reference-series${query}`)).items
  },
  subscribeReference(seriesCode: string) {
    return request<ReferenceSeries>(`${endpoints.marketData}/api/v1/reference-subscriptions`, {
      method: 'POST', body: JSON.stringify({ seriesCode }),
    })
  },
  unsubscribe(instrumentToken: number) {
    return request<void>(`${endpoints.marketData}/api/v1/subscriptions/${instrumentToken}`, { method: 'DELETE' })
  },
  refreshMaster() {
    return request<{ status: string }>(`${endpoints.marketData}/api/v1/instruments/refresh`, { method: 'POST' })
  },
  marketData(instrumentToken: number, date: string) {
    return request<MarketDataSnapshot>(`${endpoints.marketData}/api/v1/instruments/${instrumentToken}/market-data?date=${encodeURIComponent(date)}`)
  },
}

async function request<T>(url: string, init?: RequestInit): Promise<T> {
  const response = await fetch(url, {
    ...init,
    headers: { 'Content-Type': 'application/json', Accept: 'application/json', ...init?.headers },
  })
  if (!response.ok) {
    const body = await response.json().catch(() => ({ error: response.statusText }))
    throw new Error(body.error ?? `Request failed (${response.status})`)
  }
  if (response.status === 204) return undefined as T
  return response.json() as Promise<T>
}

export const tradeApi = {
  async list(search = ''): Promise<Trade[]> {
    const result = await request<{ items: Trade[] }>(`${endpoints.trades}/api/v1/trades?search=${encodeURIComponent(search)}`)
    return result.items
  },
  create(input: TradeInput) {
    return request<Trade>(`${endpoints.trades}/api/v1/trades`, { method: 'POST', body: JSON.stringify(input) })
  },
  update(id: string, input: TradeInput) {
    return request<Trade>(`${endpoints.trades}/api/v1/trades/${id}`, { method: 'PUT', body: JSON.stringify(input) })
  },
  remove(id: string) {
    return request<void>(`${endpoints.trades}/api/v1/trades/${id}`, { method: 'DELETE' })
  },
}

export async function serviceHealth(baseUrl: string): Promise<number> {
  const started = performance.now()
  const response = await fetch(`${baseUrl}/health/ready`, { signal: AbortSignal.timeout(2500) })
  if (!response.ok) throw new Error('not ready')
  return Math.round(performance.now() - started)
}

export async function priceTrade(payload: Record<string, unknown>) {
  return request<Record<string, unknown>>(`${endpoints.pricing}/api/v1/prices`, { method: 'POST', body: JSON.stringify(payload) })
}

export async function calculateRisk(payload: Record<string, unknown>) {
  return request<Record<string, unknown>>(`${endpoints.risk}/api/v1/risk/greeks`, { method: 'POST', body: JSON.stringify(payload) })
}
