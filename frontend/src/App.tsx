import { FormEvent, useCallback, useEffect, useMemo, useState } from 'react'
import { calculateRisk, endpoints, priceTrade, serviceHealth, tradeApi } from './api/client'
import type { ServiceState, Trade, TradeInput, TradeStatus } from './types'

type Screen = 'overview' | 'trades' | 'pricing' | 'risk' | 'services'

const navigation: { id: Screen; label: string; glyph: string }[] = [
  { id: 'overview', label: 'Overview', glyph: '⌂' },
  { id: 'trades', label: 'Trade Library', glyph: '≡' },
  { id: 'pricing', label: 'Pricing Lab', glyph: 'ƒ' },
  { id: 'risk', label: 'Risk Studio', glyph: '△' },
  { id: 'services', label: 'Services', glyph: '⋮' },
]

const blankTrade: TradeInput = {
  name: '', productType: 'EQUITY_OPTION_EUROPEAN', underlying: 'NIFTY', currency: 'INR',
  notional: 1_000_000, status: 'DRAFT', economics: { optionType: 'CALL', strike: 25000 },
}

function formatMoney(value: number, currency = 'USD') {
  return new Intl.NumberFormat('en-IN', { style: 'currency', currency, maximumFractionDigits: 0 }).format(value)
}

function statusClass(status: string) { return `status status-${status.toLowerCase()}` }

export default function App() {
  const [screen, setScreen] = useState<Screen>('overview')
  const [trades, setTrades] = useState<Trade[]>([])
  const [loading, setLoading] = useState(true)
  const [notice, setNotice] = useState<string>()
  const [services, setServices] = useState<ServiceState[]>([
    { id: 'trade-library', name: 'Trade Library', endpoint: endpoints.trades, status: 'checking' },
    { id: 'pricing', name: 'Pricing', endpoint: endpoints.pricing, status: 'checking' },
    { id: 'risk', name: 'Risk', endpoint: endpoints.risk, status: 'checking' },
    { id: 'stream', name: 'Market Stream', endpoint: endpoints.stream, status: 'offline' },
  ])

  const refreshTrades = useCallback(async (search = '') => {
    setLoading(true)
    try { setTrades(await tradeApi.list(search)); setNotice(undefined) }
    catch (error) { setNotice(error instanceof Error ? error.message : 'Trade Library unavailable') }
    finally { setLoading(false) }
  }, [])

  const checkServices = useCallback(async () => {
    const checks = await Promise.all([
      ['trade-library', endpoints.trades], ['pricing', endpoints.pricing], ['risk', endpoints.risk],
    ].map(async ([id, endpoint]) => {
      try { return { id, status: 'ready' as const, latency: await serviceHealth(endpoint) } }
      catch { return { id, status: 'offline' as const } }
    }))
    setServices(current => current.map(service => {
      const check = checks.find(item => item.id === service.id)
      return check ? { ...service, ...check } as ServiceState : service
    }))
  }, [])

  useEffect(() => { void refreshTrades(); void checkServices() }, [refreshTrades, checkServices])

  const activeTrades = trades.filter(trade => trade.status === 'ACTIVE').length
  const grossNotional = trades.reduce((sum, trade) => sum + Math.abs(trade.notional), 0)
  const readyServices = services.filter(service => service.status === 'ready').length

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand"><span className="brand-mark">A</span><span><strong>ATLAS</strong><small>TRADING SYSTEMS</small></span></div>
        <nav>{navigation.map(item => <button key={item.id} className={screen === item.id ? 'active' : ''} onClick={() => setScreen(item.id)}><span>{item.glyph}</span>{item.label}</button>)}</nav>
        <div className="sidebar-foot"><span className="pulse" /><div><strong>LOCAL WORKSTATION</strong><small>{readyServices}/3 core services ready</small></div></div>
      </aside>

      <main>
        <header className="topbar">
          <div><span className="eyebrow">AUTOMATED TRADING / {screen.toUpperCase()}</span><h1>{navigation.find(item => item.id === screen)?.label}</h1></div>
          <div className="market-clock"><span>MARKET</span><strong>{new Date().toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' })}</strong><small>Asia / Kolkata</small></div>
        </header>
        {notice && <div className="notice"><strong>Connection note</strong><span>{notice}. Start TradeLibraryService on port 8101 to enable live CRUD.</span><button onClick={() => setNotice(undefined)}>×</button></div>}

        {screen === 'overview' && <Overview trades={trades} active={activeTrades} notional={grossNotional} services={services} navigate={setScreen} />}
        {screen === 'trades' && <TradeLibrary trades={trades} loading={loading} refresh={refreshTrades} notify={setNotice} />}
        {screen === 'pricing' && <AnalyticsPanel kind="pricing" trades={trades} notify={setNotice} />}
        {screen === 'risk' && <AnalyticsPanel kind="risk" trades={trades} notify={setNotice} />}
        {screen === 'services' && <Services services={services} refresh={checkServices} />}
      </main>
    </div>
  )
}

function Overview({ trades, active, notional, services, navigate }: { trades: Trade[]; active: number; notional: number; services: ServiceState[]; navigate: (screen: Screen) => void }) {
  return <section className="screen overview">
    <div className="hero-panel"><div><span className="eyebrow">CONTROL PLANE</span><h2>One workstation.<br/><em>Every trading service.</em></h2><p>Manage your trade inventory, compare pricing models and inspect risk from a single local boundary.</p><button className="primary" onClick={() => navigate('trades')}>Open trade library <span>→</span></button></div><div className="signal-orbit"><div className="orbit orbit-one"/><div className="orbit orbit-two"/><div className="orbit-core"><strong>{active}</strong><span>ACTIVE<br/>TRADES</span></div></div></div>
    <div className="metric-grid">
      <Metric label="Trade inventory" value={String(trades.length).padStart(2, '0')} detail={`${active} active`} accent="lime" />
      <Metric label="Gross notional" value={notional ? `₹${(notional / 1_000_000).toFixed(1)}M` : '—'} detail="Across currencies" accent="cyan" />
      <Metric label="Core services" value={`${services.filter(s => s.status === 'ready').length}/3`} detail="Live readiness" accent="amber" />
    </div>
    <div className="two-column"><div className="panel"><PanelHead title="Recently updated" action="View library" onClick={() => navigate('trades')} />{trades.slice(0, 5).map(trade => <div className="activity-row" key={trade.id}><span className="instrument">{trade.underlying.slice(0, 3)}</span><div><strong>{trade.name}</strong><small>{trade.productType.replaceAll('_', ' ')}</small></div><span className={statusClass(trade.status)}>{trade.status}</span><time>{trade.updatedAt?.slice(11, 16) ?? '—'}</time></div>)}{trades.length === 0 && <Empty text="No trades have been added yet." />}</div><div className="panel"><PanelHead title="Service fabric" action="Inspect" onClick={() => navigate('services')} />{services.map(service => <div className="service-row" key={service.id}><span className={`service-dot ${service.status}`} /><div><strong>{service.name}</strong><small>{service.endpoint}</small></div><span>{service.status === 'ready' ? `${service.latency} ms` : service.status}</span></div>)}</div></div>
  </section>
}

function Metric({ label, value, detail, accent }: { label: string; value: string; detail: string; accent: string }) { return <div className={`metric ${accent}`}><span>{label}</span><strong>{value}</strong><small>{detail}</small><i /></div> }
function PanelHead({ title, action, onClick }: { title: string; action: string; onClick: () => void }) { return <div className="panel-head"><h3>{title}</h3><button onClick={onClick}>{action} →</button></div> }
function Empty({ text }: { text: string }) { return <div className="empty"><span>◇</span><p>{text}</p></div> }

function TradeLibrary({ trades, loading, refresh, notify }: { trades: Trade[]; loading: boolean; refresh: (search?: string) => Promise<void>; notify: (text?: string) => void }) {
  const [search, setSearch] = useState('')
  const [editing, setEditing] = useState<Trade | null | 'new'>(null)
  const visible = useMemo(() => trades, [trades])
  return <section className="screen">
    <div className="actionbar"><label className="search"><span>⌕</span><input placeholder="Search name, underlying or product…" value={search} onChange={event => { setSearch(event.target.value); void refresh(event.target.value) }} /></label><button className="secondary" onClick={() => void refresh(search)}>Refresh</button><button className="primary" onClick={() => setEditing('new')}>＋ New trade</button></div>
    <div className="panel table-panel"><div className="table-summary"><div><strong>{visible.length}</strong><span>records in library</span></div><span>Version-controlled trade economics</span></div><div className="trade-table-wrap"><table><thead><tr><th>Trade</th><th>Product</th><th>Underlying</th><th>Notional</th><th>Status</th><th>Version</th><th>Updated</th><th /></tr></thead><tbody>{visible.map(trade => <tr key={trade.id}><td><strong>{trade.name}</strong><small>{trade.id.slice(0, 8)}</small></td><td>{trade.productType.replaceAll('_', ' ')}</td><td><span className="ticker">{trade.underlying}</span></td><td>{formatMoney(trade.notional, trade.currency)}</td><td><span className={statusClass(trade.status)}>{trade.status}</span></td><td>v{trade.version}</td><td>{trade.updatedAt?.replace('T', ' ').slice(0, 16)}</td><td><button className="icon-button" aria-label={`Edit ${trade.name}`} onClick={() => setEditing(trade)}>•••</button></td></tr>)}</tbody></table>{loading && <div className="loading-line" />}{!loading && !visible.length && <Empty text="No matching trades. Create the first trade to begin." />}</div></div>
    {editing && <TradeEditor trade={editing === 'new' ? undefined : editing} close={() => setEditing(null)} saved={() => { setEditing(null); void refresh(search) }} notify={notify} />}
  </section>
}

function TradeEditor({ trade, close, saved, notify }: { trade?: Trade; close: () => void; saved: () => void; notify: (text?: string) => void }) {
  const [form, setForm] = useState<TradeInput>(trade ? { ...trade } : blankTrade)
  const [economics, setEconomics] = useState(JSON.stringify(form.economics, null, 2))
  const [saving, setSaving] = useState(false)
  const update = (key: keyof TradeInput, value: unknown) => setForm(current => ({ ...current, [key]: value }))
  const submit = async (event: FormEvent) => {
    event.preventDefault(); setSaving(true)
    try {
      const input = { ...form, version: trade?.version, economics: JSON.parse(economics) }
      trade ? await tradeApi.update(trade.id, input) : await tradeApi.create(input)
      saved()
    } catch (error) { notify(error instanceof Error ? error.message : 'Could not save trade') }
    finally { setSaving(false) }
  }
  const remove = async () => { if (!trade || !confirm(`Delete ${trade.name}?`)) return; try { await tradeApi.remove(trade.id); saved() } catch (error) { notify(error instanceof Error ? error.message : 'Delete failed') } }
  return <div className="drawer-backdrop" onMouseDown={event => event.target === event.currentTarget && close()}><form className="drawer" onSubmit={submit}><div className="drawer-head"><div><span className="eyebrow">{trade ? `TRADE ${trade.id.slice(0, 8)}` : 'NEW RECORD'}</span><h2>{trade ? 'Edit trade' : 'Add a trade'}</h2></div><button type="button" onClick={close}>×</button></div><div className="form-grid"><label className="span-two">Trade name<input required value={form.name} onChange={e => update('name', e.target.value)} /></label><label className="span-two">Product type<select value={form.productType} onChange={e => update('productType', e.target.value)}><option>EQUITY_OPTION_EUROPEAN</option><option>EQUITY_FORWARD</option><option>FX_FORWARD</option><option>RATE_SWAP</option><option>COMMODITY_FUTURE</option></select></label><label>Underlying<input required value={form.underlying} onChange={e => update('underlying', e.target.value.toUpperCase())} /></label><label>Currency<input required maxLength={3} value={form.currency} onChange={e => update('currency', e.target.value.toUpperCase())} /></label><label>Notional<input required type="number" value={form.notional} onChange={e => update('notional', Number(e.target.value))} /></label><label>Status<select value={form.status} onChange={e => update('status', e.target.value as TradeStatus)}><option>DRAFT</option><option>ACTIVE</option><option>MATURED</option><option>CANCELLED</option></select></label><label className="span-two">Economics JSON<textarea rows={9} value={economics} onChange={e => setEconomics(e.target.value)} spellCheck={false} /></label></div><div className="drawer-actions">{trade && <button type="button" className="danger" onClick={remove}>Delete trade</button>}<span/><button type="button" className="secondary" onClick={close}>Cancel</button><button className="primary" disabled={saving}>{saving ? 'Saving…' : 'Save trade'}</button></div></form></div>
}

function AnalyticsPanel({ kind, trades, notify }: { kind: 'pricing' | 'risk'; trades: Trade[]; notify: (text?: string) => void }) {
  const [tradeId, setTradeId] = useState(trades[0]?.id ?? '')
  const [model, setModel] = useState(kind === 'pricing' ? 'black-scholes' : 'central-bump')
  const [result, setResult] = useState<Record<string, unknown>>()
  const [running, setRunning] = useState(false)
  const run = async () => { setRunning(true); try { const payload = { tradeId, valuationTime: new Date().toISOString(), model, greeks: ['delta', 'gamma', 'vega', 'theta', 'rho'] }; setResult(kind === 'pricing' ? await priceTrade(payload) : await calculateRisk(payload)) } catch (error) { notify(error instanceof Error ? error.message : `${kind} service unavailable`) } finally { setRunning(false) } }
  return <section className="screen lab"><div className="lab-intro"><span className="eyebrow">{kind === 'pricing' ? 'MODEL COMPARISON' : 'SENSITIVITY ENGINE'}</span><h2>{kind === 'pricing' ? 'Price a persisted trade.' : 'Measure what moves the book.'}</h2><p>{kind === 'pricing' ? 'Select a trade, valuation time and registered model. Every result remains tied to the exact trade version and market snapshot.' : 'Request Greeks through the Risk service using analytic or bump-and-revalue methods.'}</p></div><div className="two-column lab-grid"><div className="panel lab-form"><h3>Run configuration</h3><label>Trade<select value={tradeId} onChange={e => setTradeId(e.target.value)}><option value="">Select trade…</option>{trades.map(t => <option key={t.id} value={t.id}>{t.name} · {t.underlying}</option>)}</select></label><label>{kind === 'pricing' ? 'Pricing model' : 'Greek method'}<select value={model} onChange={e => setModel(e.target.value)}>{kind === 'pricing' ? <><option value="black-scholes">Black-Scholes</option><option value="binomial-tree">Binomial tree</option><option value="trinomial-tree">Trinomial tree</option><option value="monte-carlo">Monte Carlo</option></> : <><option value="central-bump">Central bump</option><option value="analytic">Analytic</option><option value="automatic-differentiation">Automatic differentiation</option></>}</select></label><label>Valuation date<input type="datetime-local" defaultValue={new Date().toISOString().slice(0,16)} /></label><button className="primary wide" disabled={!tradeId || running} onClick={() => void run()}>{running ? 'Running…' : kind === 'pricing' ? 'Calculate price →' : 'Calculate risk →'}</button></div><div className="panel result-panel"><span className="eyebrow">LATEST RESULT</span>{result ? <pre>{JSON.stringify(result, null, 2)}</pre> : <div className="result-empty"><div>∿</div><h3>Ready for a calculation</h3><p>Results from the {kind} service will appear here with model and snapshot diagnostics.</p></div>}</div></div></section>
}

function Services({ services, refresh }: { services: ServiceState[]; refresh: () => Promise<void> }) {
  return <section className="screen"><div className="actionbar right"><button className="primary" onClick={() => void refresh()}>Check all services</button></div><div className="service-grid">{services.map(service => <article className="service-card" key={service.id}><div className="service-card-head"><span className={`service-dot ${service.status}`} /><span className={statusClass(service.status)}>{service.status}</span></div><span className="eyebrow">SERVICE NODE</span><h2>{service.name}</h2><code>{service.endpoint}</code><div className="service-stats"><div><span>Readiness</span><strong>{service.status === 'ready' ? 'PASS' : '—'}</strong></div><div><span>Round trip</span><strong>{service.latency ? `${service.latency} ms` : '—'}</strong></div></div></article>)}</div></section>
}
