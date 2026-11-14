import { useEffect, useState } from 'react'
import { audition, cancelLoad, chooseSample, readSnapshot, selectPad, setMasterGain, type Snapshot } from './bridge'

const initial: Snapshot = {
  schemaVersion: 2, selectedPad: 0,
  pads: Array.from({ length: 16 }, (_, index) => ({
    index, midiNote: index + 36, status: 'empty', sampleName: ''
  })),
  sampleName: '', status: 'empty', message: '', durationSeconds: 0,
  sampleRate: 0, masterGain: 1
}

export default function App() {
  const [snapshot, setSnapshot] = useState<Snapshot>(initial)
  useEffect(() => {
    let alive = true
    const refresh = async () => {
      try {
        const next = await readSnapshot()
        if (alive) setSnapshot(next)
      } catch {
        if (alive) setSnapshot(current => ({
          ...current, status: 'error', message: 'Native state unavailable.'
        }))
      }
    }
    void refresh()
    const timer = window.setInterval(() => void refresh(), 250)
    return () => { alive = false; window.clearInterval(timer) }
  }, [])

  const selected = snapshot.selectedPad
  const selectedLabel = String(selected + 1).padStart(2, '0')
  return <main className="chassis">
    <header><div><strong>HYPERREAL</strong><small>16 PAD DRUM SAMPLER</small></div><span>PHASE 3 FOUNDATION</span></header>
    <section className="display" aria-live="polite">
      <div className="display-row"><span>PAD {selectedLabel} / MIDI {selected + 36}</span><span>{snapshot.status.toUpperCase()}</span></div>
      <h1>{snapshot.sampleName || 'NO SAMPLE LOADED'}</h1>
      <div className="display-row"><span>{snapshot.sampleRate ? (snapshot.sampleRate/1000).toFixed(1)+' KHZ' : '-- KHZ'}</span><span>{snapshot.durationSeconds ? snapshot.durationSeconds.toFixed(2)+' SEC' : '-- SEC'}</span></div>
      <p>{snapshot.message || 'SELECT A PAD AND LOAD A SAMPLE'}</p>
    </section>
    <nav><button onClick={() => void chooseSample()} disabled={snapshot.status === 'loading'}>LOAD TO PAD {selectedLabel}</button><button onClick={() => void audition(selected)} disabled={snapshot.status !== 'ready'}>AUDITION PAD {selectedLabel}</button>{snapshot.status === 'loading' && <button onClick={() => void cancelLoad(selected)}>CANCEL LOAD</button>}</nav>
    <label className="gain">MASTER GAIN <input type="range" min="0" max="1" step="0.01" value={snapshot.masterGain} onChange={event => { const value = Number(event.target.value); setSnapshot(current => ({ ...current, masterGain:value })); void setMasterGain(value) }} /><output>{Math.round(snapshot.masterGain * 100)}%</output></label>
    <section className="pads" aria-label="Sixteen drum pads">{snapshot.pads.map((pad) =>
      <button key={pad.midiNote}
        className={'pad '+(pad.index === selected ? 'selected ' : '')+(pad.status === 'ready' ? 'loaded' : '')}
        onClick={() => void selectPad(pad.index)}
        onDoubleClick={() => { if (pad.status === 'ready') void audition(pad.index) }}
        aria-label={'Select pad '+(pad.index+1)+', MIDI note '+pad.midiNote+', '+pad.status}
        aria-pressed={pad.index === selected}>
        <span>{String(pad.index+1).padStart(2,'0')} · {pad.status.toUpperCase()}</span>
        <b>{pad.sampleName || 'UNASSIGNED'}</b>
        <span>MIDI {pad.midiNote}</span>
      </button>)}</section>
    <footer><span>SELECT A PAD, THEN LOAD OR AUDITION</span><span>4 VOICES PER PAD</span></footer>
  </main>
}
