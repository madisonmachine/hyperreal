import { useEffect, useState } from 'react'
import { setPadFilter, type Snapshot } from './bridge'

type Field = 'mode'|'cutoff'|'resonance'|'depth'|'attack'|'decay'|'sustain'|'release'
type Values = Record<Field, number>
const fromSnapshot = (snapshot: Snapshot): Values => ({
  mode: snapshot.filterMode, cutoff: snapshot.filterCutoff,
  resonance: snapshot.filterResonance, depth: snapshot.filterDepth,
  attack: snapshot.filterAttack, decay: snapshot.filterDecay,
  sustain: snapshot.filterSustain, release: snapshot.filterRelease
})
const cutoffPosition = (cutoff: number) => Math.log(cutoff / 20) / Math.log(1000)
const cutoffAt = (position: number) => 20 * Math.pow(1000, position)

export default function FilterEditor({ snapshot }: { snapshot: Snapshot }) {
  const [values, setValues] = useState<Values>(() => fromSnapshot(snapshot))
  const [error, setError] = useState('')
  useEffect(() => { setValues(fromSnapshot(snapshot)) }, [
    snapshot.filterMode, snapshot.filterCutoff, snapshot.filterResonance,
    snapshot.filterDepth, snapshot.filterAttack, snapshot.filterDecay,
    snapshot.filterSustain, snapshot.filterRelease
  ])
  const change = (field: Field, value: number) => {
    setValues(current => ({ ...current, [field]: value }))
    setError('')
    void setPadFilter(snapshot.selectedPad, field, value).then(accepted => {
      if (!accepted) { setValues(fromSnapshot(snapshot)); setError('FILTER CHANGE REJECTED') }
    }).catch(() => { setValues(fromSnapshot(snapshot)); setError('FILTER CHANGE FAILED') })
  }
  const attackX = Math.min(62, values.attack * 10)
  const decayX = attackX + Math.min(62, values.decay * 10)
  const sustainY = 72 - values.sustain * 64
  const holdX = decayX + 40
  const releaseX = holdX + Math.min(62, values.release * 10)
  const stages: { field: Field; label: string; max: number; unit: string }[] = [
    { field: 'attack', label: 'ATTACK', max: 10, unit: 's' },
    { field: 'decay', label: 'DECAY', max: 10, unit: 's' },
    { field: 'sustain', label: 'SUSTAIN', max: 1, unit: '' },
    { field: 'release', label: 'RELEASE', max: 10, unit: 's' }
  ]
  return <div className="filter-editor">
    <div className="filter-controls">
      <label>MODE <select value={values.mode} onChange={event => change('mode', Number(event.target.value))}>
        <option value={0}>OFF</option><option value={1}>LOW-PASS</option>
        <option value={2}>HIGH-PASS</option><option value={3}>BAND-PASS</option>
      </select></label>
      <label>CUTOFF
        <input type="range" min="0" max="1" step="0.001"
          value={cutoffPosition(values.cutoff)}
          onChange={event => change('cutoff', cutoffAt(Number(event.target.value)))} />
        <output>{Math.round(values.cutoff)} Hz</output>
      </label>
      <label>RESONANCE Q
        <input type="range" min="0.5" max="10" step="0.01" value={values.resonance}
          onChange={event => change('resonance', Number(event.target.value))} />
        <output>{values.resonance.toFixed(2)}</output>
      </label>
      <label>ENVELOPE DEPTH
        <input type="range" min="-48" max="48" step="0.1" value={values.depth}
          onChange={event => change('depth', Number(event.target.value))} />
        <output>{values.depth.toFixed(1)} st</output>
      </label>
    </div>
    <span className="filter-heading">FILTER ENVELOPE</span>
    <svg viewBox="0 0 240 80" role="img" aria-label="Independent filter attack, decay, sustain and release graph">
      <path d={`M 0 72 L ${attackX} 8 L ${decayX} ${sustainY} L ${holdX} ${sustainY} L ${releaseX} 72`} />
    </svg>
    <div className="filter-controls">{stages.map(({ field, label, max, unit }) =>
      <label key={field}>{label}
        <input type="range" min="0" max={max} step="0.001" value={values[field]}
          onChange={event => change(field, Number(event.target.value))} />
        <output>{values[field].toFixed(3)}{unit}</output>
      </label>)}</div>
    <p>MODE AND ENVELOPE APPLY TO NEW NOTES · CUTOFF, Q AND DEPTH MOVE ACTIVE VOICES</p>
    {error && <p className="operation-error" role="alert">{error}</p>}
  </div>
}
