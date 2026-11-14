import { useEffect, useState } from 'react'
import { audition, chooseSample, readSnapshot, setMasterGain, type Snapshot } from './bridge'
const notes = Array.from({ length: 16 }, (_, i) => i + 36)
export default function App() {
  const [snapshot, setSnapshot] = useState<Snapshot>({ schemaVersion:1, sampleName:'', status:'empty', message:'', durationSeconds:0, sampleRate:0, masterGain:1 })
  useEffect(() => {
    let alive = true
    const refresh = async () => { try { const next = await readSnapshot(); if (alive) setSnapshot(next) }
      catch { if (alive) setSnapshot(current => ({ ...current, status:'error', message:'Native state unavailable.' })) } }
    void refresh()
    const timer = window.setInterval(() => void refresh(), 250)
    return () => { alive = false; window.clearInterval(timer) }
  }, [])
  return <main className="chassis">
    <header><div><strong>HYPERREAL</strong><small>16 PAD DRUM SAMPLER</small></div><span>PHASE 2 PROTOTYPE</span></header>
    <section className="display" aria-live="polite">
      <div className="display-row"><span>PAD 01 / MIDI 36</span><span>{snapshot.status.toUpperCase()}</span></div>
      <h1>{snapshot.sampleName || 'NO SAMPLE LOADED'}</h1>
      <div className="display-row"><span>{snapshot.sampleRate ? (snapshot.sampleRate/1000).toFixed(1)+' KHZ' : '-- KHZ'}</span><span>{snapshot.durationSeconds ? snapshot.durationSeconds.toFixed(2)+' SEC' : '-- SEC'}</span></div>
      <p>{snapshot.message || 'LOAD WAV OR AIFF TO BEGIN'}</p>
    </section>
    <nav><button onClick={() => void chooseSample()} disabled={snapshot.status === 'loading'}>LOAD SAMPLE</button><button onClick={() => void audition()} disabled={snapshot.status !== 'ready'}>AUDITION</button></nav>
    <label className="gain">MASTER GAIN <input type="range" min="0" max="1" step="0.01" value={snapshot.masterGain} onChange={event => { const value = Number(event.target.value); setSnapshot(current => ({ ...current, masterGain:value })); void setMasterGain(value) }} /><output>{Math.round(snapshot.masterGain * 100)}%</output></label>
    <section className="pads" aria-label="Sixteen drum pads">{notes.map((note,i) =>
      <button key={note} className={'pad '+(i===0?'active':'inactive')} disabled={i!==0 || snapshot.status!=='ready'} onClick={() => void audition()} aria-label={'Pad '+(i+1)+', MIDI note '+note}>
        <span>{String(i+1).padStart(2,'0')}</span><b>{i===0 ? (snapshot.sampleName || 'EMPTY') : 'UNASSIGNED'}</b><span>MIDI {note}</span>
      </button>)}</section>
    <footer><span>LOCAL INSTRUMENT</span><span>PAD 01 AUDIO SLICE</span></footer>
  </main>
}