import { useEffect, useState } from 'react'
import { setPadLfo, type Snapshot } from './bridge'

type Field = 'destination'|'shape'|'rate'|'depth'|'phase'
type Values = Record<Field, number>
const fromSnapshot = (snapshot: Snapshot): Values => ({
  destination: snapshot.lfoDestination, shape: snapshot.lfoShape,
  rate: snapshot.lfoRate, depth: snapshot.lfoDepth, phase: snapshot.lfoPhaseMode
})
const ratePosition = (rate: number) => Math.log(rate / 0.05) / Math.log(400)
const rateAt = (position: number) => 0.05 * Math.pow(400, position)
const waveAt = (shape: number, phase: number) => shape === 0
  ? Math.sin(2 * Math.PI * phase)
  : shape === 1 ? 1 - 4 * Math.abs(phase - 0.5)
  : shape === 2 ? (phase < 0.5 ? 1 : -1) : 2 * phase - 1

export default function LfoEditor({ snapshot }: { snapshot: Snapshot }) {
  const [values, setValues] = useState<Values>(() => fromSnapshot(snapshot))
  const [error, setError] = useState('')
  useEffect(() => { setValues(fromSnapshot(snapshot)) }, [
    snapshot.selectedPad, snapshot.lfoDestination, snapshot.lfoShape, snapshot.lfoRate,
    snapshot.lfoDepth, snapshot.lfoPhaseMode
  ])
  const change = (field: Field, value: number) => {
    setValues(current => ({ ...current, [field]: value }))
    setError('')
    void setPadLfo(snapshot.selectedPad, field, value).then(accepted => {
      if (!accepted) { setValues(fromSnapshot(snapshot)); setError('LFO CHANGE REJECTED') }
    }).catch(() => { setValues(fromSnapshot(snapshot)); setError('LFO CHANGE FAILED') })
  }
  const preview = Array.from({ length: 97 }, (_, index) => {
    const x = index * 2.5
    const phase = (index / 48) % 1
    const y = 40 - waveAt(values.shape, phase) * 30
    return `${index === 0 ? 'M' : 'L'} ${x} ${y}`
  }).join(' ')
  return <div className="lfo-editor">
    <svg viewBox="0 0 240 80" role="img" aria-label="Selected LFO waveform preview">
      <path d={preview} />
    </svg>
    <div className="lfo-controls">
      <label>DESTINATION <select value={values.destination}
        onChange={event => change('destination', Number(event.target.value))}>
        <option value={0}>OFF</option><option value={1}>PITCH</option>
        <option value={2}>FILTER CUTOFF</option>
      </select></label>
      <label>SHAPE <select value={values.shape}
        onChange={event => change('shape', Number(event.target.value))}>
        <option value={0}>SINE</option><option value={1}>TRIANGLE</option>
        <option value={2}>SQUARE</option><option value={3}>SAW</option>
      </select></label>
      <label>PHASE <select value={values.phase}
        onChange={event => change('phase', Number(event.target.value))}>
        <option value={0}>RESET EACH NOTE</option><option value={1}>FREE-RUNNING</option>
      </select></label>
      <label>RATE
        <input type="range" min="0" max="1" step="0.001" value={ratePosition(values.rate)}
          onChange={event => change('rate', rateAt(Number(event.target.value)))} />
        <output>{values.rate.toFixed(2)} Hz</output>
      </label>
      <label>DEPTH
        <input type="range" min="0" max="24" step="0.1" value={values.depth}
          onChange={event => change('depth', Number(event.target.value))} />
        <output>{values.depth.toFixed(1)} st</output>
      </label>
    </div>
    <p>FREE-RUNNING PHASE CONTINUES BETWEEN NOTES. RATE IS IN HZ, WITHOUT TEMPO SYNC.</p>
    {error && <p className="operation-error" role="alert">{error}</p>}
  </div>
}
