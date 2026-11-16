import { setPadAmplitude, type Snapshot } from './bridge'
import { useShapingControls } from './useShapingControls'
import RotaryControl from './RotaryControl'

type Field = 'attack' | 'decay' | 'sustain' | 'release'
type Values = Record<Field, number>
const fromSnapshot = (snapshot: Snapshot): Values => ({
  attack: snapshot.ampAttack, decay: snapshot.ampDecay,
  sustain: snapshot.ampSustain, release: snapshot.ampRelease
})

export default function AmplitudeEditor({ snapshot }: { snapshot: Snapshot }) {
  const { values, error, change } = useShapingControls<Values>(
    snapshot, fromSnapshot, setPadAmplitude, 'ENVELOPE')
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
      <RotaryControl key={field} label={label} value={values[field]} min={0} max={max}
        step={0.001} onChange={value => change(field, value)}
        format={value => `${value.toFixed(3)}${unit}`} accent="#d5a9e6" />)}</div>
    <p>ONE-SHOT RELEASE ENDS AT THE SAMPLE BOUNDARY. GATE RELEASE STARTS AT NOTE-OFF.</p>
    {error && <p className="operation-error" role="alert">{error}</p>}
  </div>
}
