import { useRef, type CSSProperties, type KeyboardEvent, type PointerEvent } from 'react'

type Props = {
  label: string
  value: number
  min: number
  max: number
  step: number
  onChange: (value: number) => void
  format?: (value: number) => string
  scale?: 'linear' | 'log'
  accent?: string
  disabled?: boolean
}

const clamp = (value: number, low: number, high: number) => Math.min(high, Math.max(low, value))

export default function RotaryControl({ label, value, min, max, step, onChange,
  format = String, scale = 'linear', accent = '#edbd79', disabled = false }: Props) {
  const drag = useRef<{ pointer: number; y: number; position: number } | null>(null)
  const safeMin = Number.isFinite(min) ? min : 0
  const safeMax = Number.isFinite(max) ? Math.max(safeMin, max) : safeMin
  const logarithmic = scale === 'log' && safeMin > 0 && safeMax > safeMin
  const toPosition = (number: number) => logarithmic
    ? Math.log(clamp(number, safeMin, safeMax) / safeMin) / Math.log(safeMax / safeMin)
    : safeMax > safeMin ? (clamp(number, safeMin, safeMax) - safeMin) / (safeMax - safeMin) : 0
  const fromPosition = (position: number) => logarithmic
    ? safeMin * Math.pow(safeMax / safeMin, clamp(position, 0, 1))
    : safeMin + (safeMax - safeMin) * clamp(position, 0, 1)
  const commit = (number: number) => {
    const bounded = clamp(number, safeMin, safeMax)
    const rounded = step > 0 ? safeMin + Math.round((bounded - safeMin) / step) * step : bounded
    const next = clamp(Number(rounded.toFixed(6)), safeMin, safeMax)
    if (next !== value) onChange(next)
  }
  const pointerDown = (event: PointerEvent<HTMLDivElement>) => {
    if (disabled) return
    event.preventDefault()
    event.currentTarget.focus()
    event.currentTarget.setPointerCapture(event.pointerId)
    drag.current = { pointer: event.pointerId, y: event.clientY, position: toPosition(value) }
  }
  const pointerMove = (event: PointerEvent<HTMLDivElement>) => {
    if (!drag.current || drag.current.pointer !== event.pointerId) return
    const sensitivity = event.shiftKey ? 0.00012 : 0.005
    commit(fromPosition(drag.current.position + (drag.current.y - event.clientY) * sensitivity))
  }
  const pointerEnd = (event: PointerEvent<HTMLDivElement>) => {
    if (drag.current?.pointer !== event.pointerId) return
    drag.current = null
    if (event.currentTarget.hasPointerCapture(event.pointerId))
      event.currentTarget.releasePointerCapture(event.pointerId)
  }
  const keyboard = (event: KeyboardEvent<HTMLDivElement>) => {
    if (disabled) return
    const increment = step > 0 ? step : (safeMax - safeMin) / 100
    const amount = event.shiftKey ? increment * 10 : increment
    let next: number | null = null
    if (event.key === 'ArrowUp' || event.key === 'ArrowRight') next = value + amount
    if (event.key === 'ArrowDown' || event.key === 'ArrowLeft') next = value - amount
    if (event.key === 'PageUp') next = value + increment * 10
    if (event.key === 'PageDown') next = value - increment * 10
    if (event.key === 'Home') next = safeMin
    if (event.key === 'End') next = safeMax
    if (next !== null) { event.preventDefault(); commit(next) }
  }
  const position = toPosition(value)
  const angle = (-135 + position * 270) * Math.PI / 180
  const markerX = 32 + Math.sin(angle) * 18
  const markerY = 32 - Math.cos(angle) * 18
  return <div className={'rotary-control'+(disabled ? ' is-disabled' : '')}
    role="slider" tabIndex={disabled ? -1 : 0} aria-label={label}
    aria-valuemin={safeMin} aria-valuemax={safeMax} aria-valuenow={value}
    aria-valuetext={format(value)} aria-disabled={disabled}
    style={{ '--knob-accent': accent } as CSSProperties}
    onPointerDown={pointerDown} onPointerMove={pointerMove}
    onPointerUp={pointerEnd} onPointerCancel={pointerEnd} onKeyDown={keyboard}>
    <span className="rotary-label">{label}</span>
    <svg viewBox="0 0 64 64" aria-hidden="true">
      <circle className="rotary-track" cx="32" cy="32" r="25" />
      <circle className="rotary-progress" cx="32" cy="32" r="25"
        pathLength="100" style={{ strokeDasharray: `${position * 75} 100`, opacity: position > 0 ? 1 : 0 }} />
      <circle className="rotary-cap" cx="32" cy="32" r="20" />
      <line className="rotary-indicator" x1="32" y1="32" x2={markerX} y2={markerY} />
      <circle className="rotary-center" cx="32" cy="32" r="3" />
    </svg>
    <output className="rotary-value">{format(value)}</output>
  </div>
}
