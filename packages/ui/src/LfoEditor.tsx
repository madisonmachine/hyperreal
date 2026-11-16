import { setPadLfo, type Snapshot } from './bridge'
import { useShapingControls } from './useShapingControls'
import RotaryControl from './RotaryControl'

type Field = 'destination'|'shape'|'rate'|'depth'|'phase'
type Values = Record<Field, number>
const fromSnapshot = (snapshot: Snapshot): Values => ({
  destination: snapshot.lfoDestination, shape: snapshot.lfoShape,
  rate: snapshot.lfoRate, depth: snapshot.lfoDepth, phase: snapshot.lfoPhaseMode
})
const waveAt = (shape: number, phase: number) => shape === 0
  ? Math.sin(2 * Math.PI * phase)
  : shape === 1 ? 1 - 4 * Math.abs(phase - 0.5)
  : shape === 2 ? (phase < 0.5 ? 1 : -1) : 2 * phase - 1

export default function LfoEditor({ snapshot }: { snapshot: Snapshot }) {
  const { values, error, change } = useShapingControls<Values>(
    snapshot, fromSnapshot, setPadLfo, 'LFO')
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
      <RotaryControl label="DESTINATION" value={values.destination} min={0} max={2} step={1}
        onChange={value => change('destination', value)}
        format={value => ['OFF', 'PITCH', 'CUTOFF'][value]} accent="#d2b6e4" />
      <RotaryControl label="SHAPE" value={values.shape} min={0} max={3} step={1}
        onChange={value => change('shape', value)}
        format={value => ['SINE', 'TRIANGLE', 'SQUARE', 'SAW'][value]} accent="#d2b6e4" />
      <RotaryControl label="PHASE" value={values.phase} min={0} max={1} step={1}
        onChange={value => change('phase', value)}
        format={value => ['RESET', 'FREE-RUNNING'][value]} accent="#d2b6e4" />
      <RotaryControl label="RATE" value={values.rate} min={0.05} max={20} step={0.001}
        scale="log" onChange={value => change('rate', value)}
        format={value => `${value.toFixed(2)} Hz`} accent="#d2b6e4" />
      <RotaryControl label="DEPTH" value={values.depth} min={0} max={24} step={0.1}
        onChange={value => change('depth', value)}
        format={value => `${value.toFixed(1)} st`} accent="#d2b6e4" />
    </div>
    <p>FREE-RUNNING PHASE CONTINUES BETWEEN NOTES. RATE IS IN HZ, WITHOUT TEMPO SYNC.</p>
    {error && <p className="operation-error" role="alert">{error}</p>}
  </div>
}
