import type { Trade, TradeInput } from '../types'

export const endpoints = {
  trades: import.meta.env.VITE_TRADE_LIBRARY_URL ?? '/trade-library',
  pricing: import.meta.env.VITE_PRICING_URL ?? '/pricing',
  risk: import.meta.env.VITE_RISK_URL ?? '/risk',
  stream: import.meta.env.VITE_STREAM_URL ?? `${window.location.protocol === 'https:' ? 'wss:' : 'ws:'}//${window.location.host}/stream`,
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
