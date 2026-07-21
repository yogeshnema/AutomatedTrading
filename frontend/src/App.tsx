import { FormEvent, useCallback, useEffect, useMemo, useState } from 'react'
import { calculateRisk, endpoints, marketDataApi, priceTrade, serviceHealth, tradeApi } from './api/client'
import type { MarketCandle, MarketDataSnapshot, MarketDataStatus, MarketInstrument, MarketSubscription, ReferenceSeries, ServiceState, Trade, TradeInput, TradeStatus } from './types'
import './trade-editor.css'

type Screen = 'overview' | 'market-data' | 'trades' | 'pricing' | 'risk' | 'services'

const navigation: { id: Screen; label: string; glyph: string }[] = [
  { id: 'overview', label: 'Overview', glyph: '⌂' },
  { id: 'market-data', label: 'Market Data', glyph: 'MD' },
  { id: 'trades', label: 'Trade Library', glyph: '≡' },
  { id: 'pricing', label: 'Pricing Lab', glyph: 'ƒ' },
  { id: 'risk', label: 'Risk Studio', glyph: '△' },
  { id: 'services', label: 'Services', glyph: '⋮' },
]

const blankTrade: TradeInput = {
  name: '', status: 'DRAFT', instrumentToken: 0, side: 'buy', quantity: 1,
  executionPrice: 0, impliedVolSeriesCode: 'NIFTY_CONTRACT_IV', rateSeriesCode: 'FBIL_MIBOR_ON',
}

function formatMoney(value: number, currency = 'USD') {
  return new Intl.NumberFormat('en-IN', { style: 'currency', currency, maximumFractionDigits: 0 }).format(value)
}

function statusClass(status: string) { return `status status-${status.toLowerCase()}` }

function localDate() {
  const value = new Date()
  value.setMinutes(value.getMinutes() - value.getTimezoneOffset())
  return value.toISOString().slice(0, 10)
}

export default function App() {
  const [screen, setScreen] = useState<Screen>('overview')
  const [trades, setTrades] = useState<Trade[]>([])
  const [loading, setLoading] = useState(true)
  const [notice, setNotice] = useState<string>()
  const [services, setServices] = useState<ServiceState[]>([
    { id: 'trade-library', name: 'Trade Library', endpoint: endpoints.trades, status: 'checking' },
    { id: 'pricing', name: 'Pricing', endpoint: endpoints.pricing, status: 'checking' },
    { id: 'risk', name: 'Risk', endpoint: endpoints.risk, status: 'checking' },
    { id: 'market-data', name: 'Market Data', endpoint: endpoints.marketData, status: 'checking' },
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
      ['trade-library', endpoints.trades], ['pricing', endpoints.pricing], ['risk', endpoints.risk], ['market-data', endpoints.marketData],
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
        <div className="brand"><span className="brand-mark">V</span><span><strong>VYPYAR</strong><small>TRADING SYSTEMS</small></span></div>
        <nav>{navigation.map(item => <button key={item.id} className={screen === item.id ? 'active' : ''} onClick={() => setScreen(item.id)}><span>{item.glyph}</span>{item.label}</button>)}</nav>
        <div className="sidebar-foot"><span className="pulse" /><div><strong>LOCAL WORKSTATION</strong><small>{readyServices}/4 core services ready</small></div></div>
      </aside>

      <main>
        <header className="topbar">
          <div><span className="eyebrow">AUTOMATED TRADING / {screen.toUpperCase()}</span><h1>{navigation.find(item => item.id === screen)?.label}</h1></div>
          <div className="market-clock"><span>MARKET</span><strong>{new Date().toLocaleDateString('en-IN', { day: '2-digit', month: 'short', year: 'numeric' })}</strong><small>Asia / Kolkata</small></div>
        </header>
        {notice && <div className="notice"><strong>Connection note</strong><span>{notice}</span><button onClick={() => setNotice(undefined)}>×</button></div>}

        {screen === 'overview' && <Overview trades={trades} active={activeTrades} notional={grossNotional} services={services} navigate={setScreen} />}
        {screen === 'market-data' && <MarketDataScreen notify={setNotice} />}
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
      <Metric label="Core services" value={`${services.filter(s => s.status === 'ready').length}/4`} detail="Live readiness" accent="amber" />
    </div>
    <div className="two-column"><div className="panel"><PanelHead title="Recently updated" action="View library" onClick={() => navigate('trades')} />{trades.slice(0, 5).map(trade => <div className="activity-row" key={trade.id}><span className="instrument">{trade.underlying.slice(0, 3)}</span><div><strong>{trade.name}</strong><small>{trade.productType.replaceAll('_', ' ')}</small></div><span className={statusClass(trade.status)}>{trade.status}</span><time>{trade.updatedAt?.slice(11, 16) ?? '—'}</time></div>)}{trades.length === 0 && <Empty text="No trades have been added yet." />}</div><div className="panel"><PanelHead title="Service fabric" action="Inspect" onClick={() => navigate('services')} />{services.map(service => <div className="service-row" key={service.id}><span className={`service-dot ${service.status}`} /><div><strong>{service.name}</strong><small>{service.endpoint}</small></div><span>{service.status === 'ready' ? `${service.latency} ms` : service.status}</span></div>)}</div></div>
  </section>
}

function Metric({ label, value, detail, accent }: { label: string; value: string; detail: string; accent: string }) { return <div className={`metric ${accent}`}><span>{label}</span><strong>{value}</strong><small>{detail}</small><i /></div> }
function PanelHead({ title, action, onClick }: { title: string; action: string; onClick: () => void }) { return <div className="panel-head"><h3>{title}</h3><button onClick={onClick}>{action} →</button></div> }
function Empty({ text }: { text: string }) { return <div className="empty"><span>◇</span><p>{text}</p></div> }

function MarketDataScreen({ notify }: { notify: (text?: string) => void }) {
  const [status, setStatus] = useState<MarketDataStatus>()
  const [instruments, setInstruments] = useState<MarketInstrument[]>([])
  const [subscriptions, setSubscriptions] = useState<MarketSubscription[]>([])
  const [search, setSearch] = useState('')
  const [expiry, setExpiry] = useState('')
  const [optionType, setOptionType] = useState('')
  const [loading, setLoading] = useState(true)
  const [changing, setChanging] = useState<number>()
  const [selectedToken, setSelectedToken] = useState<number>()
  const [dataDate, setDataDate] = useState(localDate)
  const [snapshot, setSnapshot] = useState<MarketDataSnapshot>()

  const load = useCallback(async () => {
    setLoading(true)
    try {
      const [nextStatus, nextInstruments, nextSubscriptions] = await Promise.all([
        marketDataApi.status(), marketDataApi.instruments({ search, expiry, optionType }), marketDataApi.subscriptions(),
      ])
      setStatus(nextStatus); setInstruments(nextInstruments); setSubscriptions(nextSubscriptions); notify(undefined)
      setSelectedToken(current => current ?? nextSubscriptions[0]?.instrumentToken ?? nextInstruments[0]?.instrumentToken)
    } catch (error) { notify(error instanceof Error ? error.message : 'Market Data service unavailable') }
    finally { setLoading(false) }
  }, [search, expiry, optionType, notify])

  useEffect(() => { void load() }, [load])
  useEffect(() => {
    if (!selectedToken) { setSnapshot(undefined); return }
    marketDataApi.marketData(selectedToken, dataDate).then(setSnapshot)
      .catch(error => notify(error instanceof Error ? error.message : 'Stored market data unavailable'))
  }, [selectedToken, dataDate, notify])

  const toggle = async (instrument: MarketInstrument) => {
    setChanging(instrument.instrumentToken)
    try {
      instrument.subscribed ? await marketDataApi.unsubscribe(instrument.instrumentToken) : await marketDataApi.subscribe(instrument.instrumentToken)
      await load()
    } catch (error) { notify(error instanceof Error ? error.message : 'Subscription update failed') }
    finally { setChanging(undefined) }
  }

  const refreshMaster = async () => {
    setLoading(true)
    try { await marketDataApi.refreshMaster(); await load() }
    catch (error) { notify(error instanceof Error ? error.message : 'Instrument master refresh failed'); setLoading(false) }
  }

  return <section className="screen market-data-screen">
    <div className="market-data-summary"><div><span className="eyebrow">KITE / NFO / NIFTY OPTIONS</span><h2>Instrument subscriptions</h2><p>Choose contracts from the daily Kite instrument master. The backend persists each selection and refreshes its one-minute candles automatically.</p></div><button className="secondary" onClick={() => void refreshMaster()} disabled={loading}>Refresh instrument master</button></div>
    <div className="metric-grid market-metrics"><Metric label="Instruments" value={status ? status.instrumentCount.toLocaleString('en-IN') : '—'} detail="NIFTY option contracts" accent="cyan" /><Metric label="Subscriptions" value={String(status?.activeSubscriptions ?? 0).padStart(2, '0')} detail={`Polling every ${status?.pollSeconds ?? 60}s`} accent="lime" /><Metric label="Worker" value={status?.workerRunning ? 'LIVE' : 'OFF'} detail={status?.instrumentMasterRefreshedAt ? `Master ${new Date(status.instrumentMasterRefreshedAt).toLocaleString('en-IN')}` : 'Awaiting first refresh'} accent="amber" /></div>
    <div className="market-layout"><div className="panel market-catalog"><div className="market-filters"><label className="search"><span>⌕</span><input aria-label="Search instruments" placeholder="Search trading symbol…" value={search} onChange={event => setSearch(event.target.value.toUpperCase())} /></label><input aria-label="Expiry" type="date" value={expiry} onChange={event => setExpiry(event.target.value)} /><select aria-label="Option type" value={optionType} onChange={event => setOptionType(event.target.value)}><option value="">Calls + puts</option><option value="CE">Calls</option><option value="PE">Puts</option></select><button className="secondary" onClick={() => void load()}>Search</button></div><div className="table-summary"><div><strong>{instruments.length}</strong><span>matching instruments</span></div><span>Daily vendor master</span></div><div className="trade-table-wrap"><table className="instrument-table"><thead><tr><th>Instrument</th><th>Expiry</th><th>Strike</th><th>Type</th><th>Token</th><th /></tr></thead><tbody>{instruments.map(instrument => <tr className={selectedToken === instrument.instrumentToken ? 'selected-market-row' : ''} key={instrument.instrumentToken}><td><strong>{instrument.tradingSymbol}</strong><small>{instrument.exchange}</small></td><td>{instrument.expiry}</td><td>{instrument.strike.toLocaleString('en-IN')}</td><td><span className={`option-chip ${instrument.optionType.toLowerCase()}`}>{instrument.optionType}</span></td><td><code>{instrument.instrumentToken}</code></td><td><div className="instrument-actions"><button className="secondary compact" onClick={() => setSelectedToken(instrument.instrumentToken)}>View</button><button className={instrument.subscribed ? 'danger compact' : 'primary compact'} disabled={changing === instrument.instrumentToken} onClick={() => void toggle(instrument)}>{changing === instrument.instrumentToken ? 'Working…' : instrument.subscribed ? 'Unsubscribe' : 'Subscribe'}</button></div></td></tr>)}</tbody></table>{loading && <div className="loading-line" />}{!loading && !instruments.length && <Empty text="No instruments match these filters." />}</div></div><aside className="panel subscription-panel"><PanelHead title="Active subscriptions" action="Refresh" onClick={() => void load()} />{subscriptions.map(item => <div className="subscription-row" key={item.instrumentToken}><button className="subscription-select" onClick={() => setSelectedToken(item.instrumentToken)}><strong>{item.tradingSymbol}</strong><small>{item.expiry} · {item.strike.toLocaleString('en-IN')} · {item.optionType}</small></button><span className={item.lastError ? 'subscription-error' : 'subscription-live'}>{item.lastError ? 'ERROR' : item.lastCandleAt ? 'CURRENT' : 'QUEUED'}</span><time>{item.lastCandleAt ? new Date(item.lastCandleAt).toLocaleTimeString('en-IN') : 'Waiting for first candle'}</time><button className="unsubscribe-link" onClick={() => void toggle({ ...item, name: 'NIFTY', subscribed: true })}>Unsubscribe</button></div>)}{!subscriptions.length && <Empty text="Subscribe to an instrument to start one-minute collection." />}</aside></div>
    <MarketDataDetail snapshot={snapshot} date={dataDate} setDate={setDataDate} />
  </section>
}

function MarketDataDetail({ snapshot, date, setDate }: { snapshot?: MarketDataSnapshot; date: string; setDate: (value: string) => void }) {
  if (!snapshot) return <div className="panel market-detail-empty"><Empty text="Select an instrument to inspect its stored candles and EOD summary." /></div>
  const summary = snapshot.summary
  const number = (value?: number | null) => value == null ? '—' : value.toLocaleString('en-IN', { maximumFractionDigits: 2 })
  return <section className="market-detail"><div className="market-detail-head"><div><span className="eyebrow">STORED MARKET HISTORY</span><h2>{snapshot.instrument.tradingSymbol}</h2><p>{summary.eodPersisted ? 'Official daily candle persisted' : 'Intraday aggregate shown until EOD reconciliation'}</p></div><label>Date<input type="date" value={date} onChange={event => setDate(event.target.value)} /></label></div><div className="market-stat-grid"><div><span>Open</span><strong>{number(summary.open)}</strong></div><div><span>Close / latest</span><strong>{number(summary.close)}</strong></div><div><span>Session range</span><strong>{number(summary.low)} – {number(summary.high)}</strong></div><div><span>Change</span><strong className={(summary.absoluteChange ?? 0) >= 0 ? 'positive' : 'negative'}>{number(summary.absoluteChange)} ({number(summary.percentChange)}%)</strong></div></div><div className="panel intraday-panel"><div className="intraday-caption"><div><h3>One-minute movement</h3><span>{summary.minuteCandleCount} stored candles · Volume {summary.volume.toLocaleString('en-IN')}</span></div><span>{summary.firstCandleAt ? new Date(summary.firstCandleAt).toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' }) : '—'} → {summary.lastCandleAt ? new Date(summary.lastCandleAt).toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' }) : '—'}</span></div><IntradayChart candles={snapshot.minuteCandles} /></div></section>
}

function IntradayChart({ candles }: { candles: MarketCandle[] }) {
  if (!candles.length) return <Empty text="No one-minute candles are stored for this date." />
  const width = 900, height = 280, left = 58, right = 18, top = 18, bottom = 34
  const low = Math.min(...candles.map(candle => candle.low)), high = Math.max(...candles.map(candle => candle.high))
  const range = Math.max(high - low, 0.01)
  const x = (index: number) => left + index / Math.max(candles.length - 1, 1) * (width - left - right)
  const y = (value: number) => top + (high - value) / range * (height - top - bottom)
  const line = candles.map((candle, index) => `${x(index)},${y(candle.close)}`).join(' ')
  const area = `${left},${height - bottom} ${line} ${width - right},${height - bottom}`
  const ticks = [high, high - range / 2, low]
  return <svg className="intraday-chart" viewBox={`0 0 ${width} ${height}`} role="img" aria-label={`One-minute close prices from ${candles[0].timestamp} to ${candles[candles.length - 1].timestamp}`}><title>One-minute intraday movement</title><desc>Close-price line with session high and low scale.</desc>{ticks.map(value => <g key={value}><line x1={left} x2={width - right} y1={y(value)} y2={y(value)} className="chart-grid"/><text x={left - 10} y={y(value) + 4} textAnchor="end">{value.toFixed(2)}</text></g>)}<polygon points={area} className="chart-area"/><polyline points={line} className="chart-line"/><circle cx={x(candles.length - 1)} cy={y(candles[candles.length - 1].close)} r="4" className="chart-last"/><text x={left} y={height - 9}>{new Date(candles[0].timestamp).toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' })}</text><text x={width - right} y={height - 9} textAnchor="end">{new Date(candles[candles.length - 1].timestamp).toLocaleTimeString('en-IN', { hour: '2-digit', minute: '2-digit' })}</text></svg>
}

function TradeLibrary({ trades, loading, refresh, notify }: { trades: Trade[]; loading: boolean; refresh: (search?: string) => Promise<void>; notify: (text?: string) => void }) {
  const [search, setSearch] = useState('')
  const [editing, setEditing] = useState<Trade | null | 'new'>(null)
  const visible = useMemo(() => trades, [trades])
  return <section className="screen">
    <div className="actionbar"><label className="search"><span>⌕</span><input placeholder="Search name, underlying or product…" value={search} onChange={event => { setSearch(event.target.value); void refresh(event.target.value) }} /></label><button className="secondary" onClick={() => void refresh(search)}>Refresh</button><button className="primary" onClick={() => setEditing('new')}>＋ New trade</button></div>
    <div className="panel table-panel"><div className="table-summary"><div><strong>{visible.length}</strong><span>records in library</span></div><span>Version-controlled trade economics</span></div><div className="trade-table-wrap"><table><thead><tr><th>Trade</th><th>Product</th><th>Underlying</th><th>Notional</th><th>Status</th><th>Version</th><th>Updated</th><th /></tr></thead><tbody>{visible.map(trade => <tr key={trade.id}><td><strong>{trade.name}</strong><small>{trade.id.slice(0, 8)}</small></td><td>{trade.productType.replaceAll('_', ' ')}</td><td><span className="ticker">{trade.underlying}</span></td><td>{formatMoney(trade.notional, trade.currency)}</td><td><span className={statusClass(trade.status)}>{trade.status}</span></td><td>v{trade.version}</td><td>{trade.updatedAt?.replace('T', ' ').slice(0, 16)}</td><td><button className="icon-button" aria-label={`Edit ${trade.name}`} onClick={() => setEditing(trade)}>•••</button></td></tr>)}</tbody></table>{loading && <div className="loading-line" />}{!loading && !visible.length && <Empty text="No matching trades. Create the first trade to begin." />}</div></div>
    {editing && <InstrumentTradeEditor trade={editing === 'new' ? undefined : editing} close={() => setEditing(null)} saved={() => { setEditing(null); void refresh(search) }} notify={notify} />}
  </section>
}

function InstrumentTradeEditor({ trade, close, saved, notify }: { trade?: Trade; close: () => void; saved: () => void; notify: (text?: string) => void }) {
  const [instrumentName, setInstrumentName] = useState(trade?.underlying ?? '')
  const [expiry, setExpiry] = useState(trade?.expiry ?? '')
  const [optionType, setOptionType] = useState(trade?.optionType ?? 'CE')
  const [strike, setStrike] = useState<number | ''>(trade?.strike ?? '')
  const [names, setNames] = useState<string[]>([])
  const [expiries, setExpiries] = useState<string[]>([])
  const [optionTypes, setOptionTypes] = useState<Array<'CE' | 'PE'>>([])
  const [strikes, setStrikes] = useState<number[]>([])
  const [contracts, setContracts] = useState<MarketInstrument[]>([])
  const [instrumentToken, setInstrumentToken] = useState(trade?.instrumentToken ?? 0)
  const [series, setSeries] = useState<ReferenceSeries[]>([])
  const [name, setName] = useState(trade?.name ?? '')
  const [status, setStatus] = useState<TradeStatus>(trade?.status ?? 'DRAFT')
  const [side, setSide] = useState<'buy' | 'sell'>(trade?.side ?? 'buy')
  const [quantity, setQuantity] = useState(trade?.quantity ?? 1)
  const [executionPrice, setExecutionPrice] = useState(trade?.executionPrice ?? 0)
  const [impliedVolSeriesCode, setImpliedVolSeriesCode] = useState(trade?.impliedVolSeriesCode ?? 'NIFTY_CONTRACT_IV')
  const [rateSeriesCode, setRateSeriesCode] = useState(trade?.rateSeriesCode ?? 'FBIL_MIBOR_ON')
  const [saving, setSaving] = useState(false)

  useEffect(() => {
    Promise.all([marketDataApi.facets(), marketDataApi.referenceSeries()]).then(([facets, values]) => {
      setNames(facets.names); setSeries(values)
      setInstrumentName(current => current || facets.names[0] || '')
    }).catch(error => notify(error instanceof Error ? error.message : 'Instrument master unavailable'))
  }, [notify])

  useEffect(() => {
    if (!instrumentName) return
    marketDataApi.facets({ name: instrumentName }).then(facets => {
      setExpiries(facets.expiries)
      setExpiry(current => facets.expiries.includes(current) ? current : facets.expiries[0] || '')
    }).catch(error => notify(error instanceof Error ? error.message : 'Could not load expiries'))
  }, [instrumentName, notify])

  useEffect(() => {
    if (!instrumentName || !expiry) return
    marketDataApi.facets({ name: instrumentName, expiry }).then(facets => {
      setOptionTypes(facets.optionTypes)
      setOptionType(current => facets.optionTypes.includes(current) ? current : facets.optionTypes[0] || 'CE')
    }).catch(error => notify(error instanceof Error ? error.message : 'Could not load option types'))
  }, [instrumentName, expiry, notify])

  useEffect(() => {
    if (!instrumentName || !expiry || !optionType) return
    marketDataApi.facets({ name: instrumentName, expiry, optionType }).then(facets => {
      setStrikes(facets.strikes)
      setStrike(current => current !== '' && facets.strikes.includes(current) ? current : facets.strikes[0] ?? '')
    }).catch(error => notify(error instanceof Error ? error.message : 'Could not load strikes'))
  }, [instrumentName, expiry, optionType, notify])

  useEffect(() => {
    if (!instrumentName || !expiry || !optionType || strike === '') return
    marketDataApi.instruments({ search: instrumentName, expiry, optionType, strike }).then(items => {
      setContracts(items)
      setInstrumentToken(current => items.some(item => item.instrumentToken === current) ? current : items[0]?.instrumentToken ?? 0)
    }).catch(error => notify(error instanceof Error ? error.message : 'Could not resolve option contract'))
  }, [instrumentName, expiry, optionType, strike, notify])

  const selected = contracts.find(item => item.instrumentToken === instrumentToken)
  const impliedVolSeries = series.filter(item => item.seriesType === 'implied_volatility')
  const rateSeries = series.filter(item => item.seriesType === 'risk_free_rate')
  const estimatedNotional = (selected?.lotSize ?? trade?.lotSize ?? 1) * quantity * executionPrice

  const submit = async (event: FormEvent) => {
    event.preventDefault()
    if (!instrumentToken) { notify('Select a valid option contract from the instrument master.'); return }
    setSaving(true)
    try {
      const input: TradeInput = { name: name || selected?.tradingSymbol || '', status, instrumentToken, side,
        quantity, executionPrice, impliedVolSeriesCode, rateSeriesCode, version: trade?.version }
      trade ? await tradeApi.update(trade.id, input) : await tradeApi.create(input)
      try {
        await Promise.all([
          marketDataApi.subscribe(instrumentToken, 'day'),
          marketDataApi.subscribeReference('NIFTY_50_EOD'),
          marketDataApi.subscribeReference(impliedVolSeriesCode),
          marketDataApi.subscribeReference(rateSeriesCode),
        ])
        notify(undefined)
      } catch (error) {
        notify(`Trade saved, but one or more pricing-data requests failed: ${error instanceof Error ? error.message : 'unknown error'}`)
      }
      saved()
    } catch (error) { notify(error instanceof Error ? error.message : 'Could not save trade') }
    finally { setSaving(false) }
  }

  const remove = async () => {
    if (!trade || !confirm(`Delete ${trade.name}?`)) return
    try { await tradeApi.remove(trade.id); saved() }
    catch (error) { notify(error instanceof Error ? error.message : 'Delete failed') }
  }

  return <div className="drawer-backdrop" onMouseDown={event => event.target === event.currentTarget && close()}><form className="drawer" onSubmit={submit}><div className="drawer-head"><div><span className="eyebrow">{trade ? `TRADE ${trade.id.slice(0, 8)}` : 'NEW OPTION TRADE'}</span><h2>{trade ? 'Edit option trade' : 'Add option trade'}</h2></div><button type="button" onClick={close}>×</button></div><div className="form-grid"><label className="span-two">Trade name<input value={name} placeholder={selected?.tradingSymbol ?? 'Generated from selected contract'} onChange={event => setName(event.target.value)} /></label><label>Instrument<select required value={instrumentName} onChange={event => setInstrumentName(event.target.value)}><option value="">Select instrument</option>{names.map(value => <option key={value}>{value}</option>)}</select></label><label>Expiry<select required value={expiry} onChange={event => setExpiry(event.target.value)}><option value="">Select expiry</option>{expiries.map(value => <option key={value}>{value}</option>)}</select></label><label>Option type<select required value={optionType} onChange={event => setOptionType(event.target.value as 'CE' | 'PE')}>{optionTypes.map(value => <option key={value} value={value}>{value === 'CE' ? 'Call' : 'Put'}</option>)}</select></label><label>Strike<select required value={strike} onChange={event => setStrike(Number(event.target.value))}><option value="">Select strike</option>{strikes.map(value => <option key={value} value={value}>{value.toLocaleString('en-IN')}</option>)}</select></label><label className="span-two">Resolved contract<select required value={instrumentToken} onChange={event => setInstrumentToken(Number(event.target.value))}><option value={0}>Select contract</option>{contracts.map(value => <option key={value.instrumentToken} value={value.instrumentToken}>{value.tradingSymbol} · lot {value.lotSize}</option>)}</select></label><label>Side<select value={side} onChange={event => setSide(event.target.value as 'buy' | 'sell')}><option value="buy">Buy</option><option value="sell">Sell</option></select></label><label>Status<select value={status} onChange={event => setStatus(event.target.value as TradeStatus)}><option>DRAFT</option><option>ACTIVE</option><option>MATURED</option><option>CANCELLED</option></select></label><label>Quantity / lots<input required min="0.000001" step="any" type="number" value={quantity} onChange={event => setQuantity(Number(event.target.value))} /></label><label>Buy / execution price<input required min="0" step="any" type="number" value={executionPrice} onChange={event => setExecutionPrice(Number(event.target.value))} /></label><label>Implied volatility source<select required value={impliedVolSeriesCode} onChange={event => setImpliedVolSeriesCode(event.target.value)}>{impliedVolSeries.map(value => <option key={value.seriesCode} value={value.seriesCode}>{value.displayName}</option>)}</select></label><label>Discount-rate source<select required value={rateSeriesCode} onChange={event => setRateSeriesCode(event.target.value)}>{rateSeries.map(value => <option key={value.seriesCode} value={value.seriesCode}>{value.displayName}</option>)}</select></label><div className="span-two dependency-note"><strong>Pricing-data requests created on save</strong><span>Option EOD · NIFTY 50 EOD · contract implied volatility · selected INR rate</span><small>Estimated consideration: {formatMoney(estimatedNotional, 'INR')}. Reference subscriptions remain “requested” until their vendor/derived adapters produce observations.</small></div></div><div className="drawer-actions">{trade && <button type="button" className="danger" onClick={remove}>Delete trade</button>}<span/><button type="button" className="secondary" onClick={close}>Cancel</button><button className="primary" disabled={saving || !instrumentToken}>{saving ? 'Saving…' : 'Save and request data'}</button></div></form></div>
}

function TradeEditor({ trade, close, saved, notify }: { trade?: Trade; close: () => void; saved: () => void; notify: (text?: string) => void }) {
  const [form, setForm] = useState<any>(trade ? { ...trade } : blankTrade)
  const [economics, setEconomics] = useState(JSON.stringify(form.economics ?? {}, null, 2))
  const [saving, setSaving] = useState(false)
  const update = (key: string, value: unknown) => setForm((current: any) => ({ ...current, [key]: value }))
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
  const [model, setModel] = useState(kind === 'pricing' ? 'black-scholes' : 'analytic')
  const [valuationDate, setValuationDate] = useState(new Date().toISOString().slice(0,16))
  const [result, setResult] = useState<Record<string, unknown>>()
  const [running, setRunning] = useState(false)
  const run = async () => { setRunning(true); try { const payload = { tradeId, valuationDate: new Date(valuationDate).toISOString(), ...(kind === 'pricing' ? { model } : { method: model }) }; setResult(kind === 'pricing' ? await priceTrade(payload) : await calculateRisk(payload)) } catch (error) { notify(error instanceof Error ? error.message : `${kind} service unavailable`) } finally { setRunning(false) } }
  return <section className="screen lab"><div className="lab-intro"><span className="eyebrow">{kind === 'pricing' ? 'MODEL COMPARISON' : 'SENSITIVITY ENGINE'}</span><h2>{kind === 'pricing' ? 'Price a persisted trade.' : 'Measure what moves the book.'}</h2><p>{kind === 'pricing' ? 'Select a trade, valuation time and registered model. Every result remains tied to the exact trade version and market snapshot.' : 'Calculate theoretical and position-scaled Greeks from the persisted trade and market snapshot.'}</p></div><div className="two-column lab-grid"><div className="panel lab-form"><h3>Run configuration</h3><label>Trade<select value={tradeId} onChange={e => setTradeId(e.target.value)}><option value="">Select trade…</option>{trades.map(t => <option key={t.id} value={t.id}>{t.name} · {t.underlying}</option>)}</select></label><label>{kind === 'pricing' ? 'Pricing model' : 'Greek method'}<select value={model} onChange={e => setModel(e.target.value)}>{kind === 'pricing' ? <><option value="black-scholes">Black-Scholes</option><option value="binomial-tree">Binomial tree</option><option value="trinomial-tree">Trinomial tree</option><option value="monte-carlo">Monte Carlo</option></> : <><option value="analytic">Analytic Black-Scholes</option><option value="central-bump" disabled>Central bump (planned)</option><option value="automatic-differentiation" disabled>Automatic differentiation (planned)</option></>}</select></label><label>Valuation date<input type="datetime-local" value={valuationDate} onChange={event => setValuationDate(event.target.value)} /></label><button className="primary wide" disabled={!tradeId || running} onClick={() => void run()}>{running ? 'Running…' : kind === 'pricing' ? 'Calculate price →' : 'Calculate risk →'}</button></div><div className="panel result-panel"><span className="eyebrow">LATEST RESULT</span>{result ? <pre>{JSON.stringify(result, null, 2)}</pre> : <div className="result-empty"><div>∿</div><h3>Ready for a calculation</h3><p>Results from the {kind} service will appear here with model and snapshot diagnostics.</p></div>}</div></div></section>
}

function Services({ services, refresh }: { services: ServiceState[]; refresh: () => Promise<void> }) {
  return <section className="screen"><div className="actionbar right"><button className="primary" onClick={() => void refresh()}>Check all services</button></div><div className="service-grid">{services.map(service => <article className="service-card" key={service.id}><div className="service-card-head"><span className={`service-dot ${service.status}`} /><span className={statusClass(service.status)}>{service.status}</span></div><span className="eyebrow">SERVICE NODE</span><h2>{service.name}</h2><code>{service.endpoint}</code><div className="service-stats"><div><span>Readiness</span><strong>{service.status === 'ready' ? 'PASS' : '—'}</strong></div><div><span>Round trip</span><strong>{service.latency ? `${service.latency} ms` : '—'}</strong></div></div></article>)}</div></section>
}
