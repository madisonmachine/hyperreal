import { setPadFilter, type Snapshot } from './bridge'
import { useShapingControls } from './useShapingControls'
import RotaryControl from './RotaryControl'

type Field = 'mode'|'cutoff'|'resonance'|'depth'|'attack'|'decay'|'sustain'|'release'
type Values = Record<Field, number>
const fromSnapshot = (snapshot: Snapshot): Values => ({
  mode: snapshot.filterMode, cutoff: snapshot.filterCutoff,
  resonance: snapshot.filterResonance, depth: snapshot.filterDepth,
  attack: snapshot.filterAttack, decay: snapshot.filterDecay,
  sustain: snapshot.filterSustain, release: snapshot.filterRelease
})

export default function FilterEditor({ snapshot }: { snapshot: Snapshot }) {
  const { values, error, change } = useShapingControls<Values>(
    snapshot, fromSnapshot, setPadFilter, 'FILTER')
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
      <RotaryControl label="MODE" value={values.mode} min={0} max={3} step={1}
        onChange={value => change('mode', value)}
        format={value => ['OFF', 'LOW-PASS', 'HIGH-PASS', 'BAND-PASS'][value]} accent="#9ed3be" />
      <RotaryControl label="CUTOFF" value={values.cutoff} min={20} max={20000} step={1}
        scale="log" onChange={value => change('cutoff', value)}
        format={value => `${Math.round(value)} Hz`} accent="#9ed3be" />
      <RotaryControl label="RESONANCE Q" value={values.resonance} min={0.5} max={10} step={0.01}
        onChange={value => change('resonance', value)} format={value => value.toFixed(2)} accent="#9ed3be" />
      <RotaryControl label="ENVELOPE DEPTH" value={values.depth} min={-48} max={48} step={0.1}
        onChange={value => change('depth', value)} format={value => `${value.toFixed(1)} st`} accent="#9ed3be" />
    </div>
    <span className="filter-heading">FILTER ENVELOPE</span>
    <svg viewBox="0 0 240 80" role="img" aria-label="Independent filter attack, decay, sustain and release graph">
      <path d={`M 0 72 L ${attackX} 8 L ${decayX} ${sustainY} L ${holdX} ${sustainY} L ${releaseX} 72`} />
    </svg>
    <div className="filter-controls">{stages.map(({ field, label, max, unit }) =>
      <RotaryControl key={field} label={label} value={values[field]} min={0} max={max}
        step={0.001} onChange={value => change(field, value)}
        format={value => `${value.toFixed(3)}${unit}`} accent="#9ed3be" />)}</div>
    <p>MODE AND ENVELOPE APPLY TO NEW NOTES. CUTOFF, Q AND DEPTH MOVE ACTIVE VOICES.</p>
    {error && <p className="operation-error" role="alert">{error}</p>}
  </div>
}
