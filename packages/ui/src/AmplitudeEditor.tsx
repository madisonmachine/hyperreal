import { useEffect, useState } from 'react'
import { setPadAmplitude, type Snapshot } from './bridge'

type Field = 'attack' | 'decay' | 'sustain' | 'release'
type Values = Record<Field, number>
const fromSnapshot = (snapshot: Snapshot): Values => ({
  attack: snapshot.ampAttack, decay: snapshot.ampDecay,
  sustain: snapshot.ampSustain, release: snapshot.ampRelease
})

export default function AmplitudeEditor({ snapshot }: { snapshot: Snapshot }) {
  const [values, setValues] = useState<Values>(() => fromSnapshot(snapshot))
  const [error, setError] = useState('')
  useEffect(() => { setValues(fromSnapshot(snapshot)) }, [
    snapshot.ampAttack, snapshot.ampDecay, snapshot.ampSustain, snapshot.ampRelease
  ])
  const change = (field: Field, value: number) => {
    setValues(current => ({ ...current, [field]: value }))
    setError('')
    void setPadAmplitude(snapshot.selectedPad, field, value).then(accepted => {
      if (!accepted) { setValues(fromSnapshot(snapshot)); setError('ENVELOPE CHANGE REJECTED') }
    }).catch(() => { setValues(fromSnapshot(snapshot)); setError('ENVELOPE CHANGE FAILED') })
  }
  const attackX = Math.min(62, values.attack * 10)
  const decayX = attackX + Math.min(62, values.decay * 10)
  const sustainY = 72 - values.sustain * 64
  const holdX = decayX + 40
  const releaseX = holdX + Math.min(62, values.release * 10)
  const controls: { field: Field; label: string; max: number; unit: string }[] = [
    { field: 'attack', label: 'ATTACK', max: 10, unit: 's' },
    { field: 'decay', label: 'DECAY', max: 10, unit: 's' },
    { field: 'sustain', label: 'SUSTAIN', max: 1, unit: '' },
    { field: 'release', label: 'RELEASE', max: 10, unit: 's' }
  ]
  return <div className="amplitude-editor">
    <svg viewBox="0 0 240 80" role="img" aria-label="Amplitude attack, decay, sustain and release graph">
      <path d={`M 0 72 L ${attackX} 8 L ${decayX} ${sustainY} L ${holdX} ${sustainY} L ${releaseX} 72`} />
    </svg>
    <div className="amplitude-controls">{controls.map(({ field, label, max, unit }) =>
      <label key={field}>{label}
        <input type="range" min="0" max={max} step="0.001" value={values[field]}
          onChange={event => change(field, Number(event.target.value))} />
        <output>{values[field].toFixed(3)}{unit}</output>
      </label>)}</div>
    <p>ONE-SHOT RELEASE ENDS AT THE SAMPLE BOUNDARY · GATE RELEASE STARTS AT NOTE-OFF</p>
    {error && <p className="operation-error" role="alert">{error}</p>}
  </div>
}
