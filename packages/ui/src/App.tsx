import { useEffect, useState } from 'react'
import { audition, cancelLoad, chooseSample, clearPad, copyPad, pastePad, readSnapshot, selectPad, setChokeGroup, setMasterGain, setPadMode, setPadMute, setPadSolo, setVelocityCurve, setVoiceLimit, swapPads, type Snapshot } from './bridge'

const initial: Snapshot = {
  schemaVersion: 2, selectedPad: 0,
  pads: Array.from({ length: 16 }, (_, index) => ({
    index, midiNote: index + 36, status: 'empty', sampleName: '', muted: false, solo: false
  })),
  sampleName: '', status: 'empty', message: '', durationSeconds: 0,
  sampleRate: 0, masterGain: 1, padLevel: 1, padMode: 0, chokeGroup: 0,
  muted: false, solo: false, clipboardReady: false,
  voiceLimit: 4, velocityCurve: 1
}

export default function App() {
  const [snapshot, setSnapshot] = useState<Snapshot>(initial)
  const [swapTarget, setSwapTarget] = useState(1)
  const [pending, setPending] = useState<{ kind: 'paste'|'swap'|'clear'; pad: number; otherPad?: number } | null>(null)
  const [operationError, setOperationError] = useState('')
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
  const confirmOperation = async () => {
    if (!pending) return
    const success = pending.kind === 'paste' ? await pastePad(pending.pad)
      : pending.kind === 'swap' ? await swapPads(pending.pad, pending.otherPad ?? pending.pad)
      : await clearPad(pending.pad)
    setOperationError(success ? '' : 'PAD OPERATION FAILED. CHECK THE SAMPLE MEMORY LIMIT.')
    setPending(null)
  }
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
    <section className="pad-settings" aria-label={'Pad '+selectedLabel+' playback settings'}>
      <label>MODE <select value={snapshot.padMode} onChange={event => { const value = Number(event.target.value); setSnapshot(current => ({ ...current, padMode:value })); void setPadMode(selected, value) }}><option value={0}>ONE-SHOT</option><option value={1}>GATE</option></select></label>
      <label>CHOKE GROUP <select value={snapshot.chokeGroup} onChange={event => { const value = Number(event.target.value); setSnapshot(current => ({ ...current, chokeGroup:value })); void setChokeGroup(selected, value) }}>{[0,1,2,3,4].map(group => <option key={group} value={group}>{group === 0 ? 'OFF' : group}</option>)}</select></label>
      <label><input type="checkbox" checked={snapshot.muted} onChange={event => { const value = event.target.checked; setSnapshot(current => ({ ...current, muted:value })); void setPadMute(selected, value) }} /> MUTE</label>
      <label><input type="checkbox" checked={snapshot.solo} onChange={event => { const value = event.target.checked; setSnapshot(current => ({ ...current, solo:value })); void setPadSolo(selected, value) }} /> SOLO</label>
      <label>VOICES <select value={snapshot.voiceLimit} onChange={event => { const value = Number(event.target.value); setSnapshot(current => ({ ...current, voiceLimit:value })); void setVoiceLimit(selected, value) }}>{[1,2,3,4].map(count => <option key={count} value={count}>{count}</option>)}</select></label>
      <label>VELOCITY <select value={snapshot.velocityCurve} onChange={event => { const value = Number(event.target.value); setSnapshot(current => ({ ...current, velocityCurve:value })); void setVelocityCurve(selected, value) }}><option value={0}>SOFT</option><option value={1}>LINEAR</option><option value={2}>HARD</option></select></label>
      <span>PAD LEVEL {Math.round(snapshot.padLevel * 100)}%</span>
    </section>
    <section className="pad-operations" aria-label="Pad operations">
      <button onClick={() => { void copyPad(selected).then(success => setOperationError(success ? '' : 'COPY FAILED')) }}>COPY PAD {selectedLabel}</button>
      <button disabled={!snapshot.clipboardReady} onClick={() => setPending({ kind: 'paste', pad: selected })}>PASTE TO {selectedLabel}</button>
      <label>SWAP WITH <select value={swapTarget} onChange={event => setSwapTarget(Number(event.target.value))}>{snapshot.pads.map(pad => <option key={pad.index} value={pad.index}>PAD {String(pad.index + 1).padStart(2, '0')}</option>)}</select></label>
      <button disabled={swapTarget === selected} onClick={() => setPending({ kind: 'swap', pad: selected, otherPad: swapTarget })}>SWAP</button>
      <button onClick={() => setPending({ kind: 'clear', pad: selected })}>CLEAR</button>
    </section>
    {pending && <section className="operation-confirm"><span>CONFIRM {pending.kind.toUpperCase()} PAD {String(pending.pad + 1).padStart(2, '0')}{pending.otherPad !== undefined ? ' WITH '+String(pending.otherPad + 1).padStart(2, '0') : ''}?</span><button onClick={() => void confirmOperation()}>CONFIRM</button><button onClick={() => setPending(null)}>CANCEL</button></section>}
    {operationError && <p className="operation-error" role="alert">{operationError}</p>}
    <section className="pads" aria-label="Sixteen drum pads">{snapshot.pads.map((pad) =>
      <button key={pad.midiNote}
        data-pad-index={pad.index}
        className={'pad '+(pad.index === selected ? 'selected ' : '')+(pad.status === 'ready' ? 'loaded ' : '')+(pad.muted ? 'muted ' : '')+(pad.solo ? 'solo' : '')}
        onClick={() => { setPending(null); setOperationError(''); void selectPad(pad.index) }}
        onDoubleClick={() => { if (pad.status === 'ready') void audition(pad.index) }}
        aria-label={'Select pad '+(pad.index+1)+', MIDI note '+pad.midiNote+', '+pad.status+(pad.muted ? ', muted' : '')+(pad.solo ? ', solo' : '')}
        aria-pressed={pad.index === selected}>
        <span>{String(pad.index+1).padStart(2,'0')} · {pad.muted ? 'MUTED' : pad.solo ? 'SOLO' : pad.status.toUpperCase()}</span>
        <b>{pad.sampleName || 'UNASSIGNED'}</b>
        <span>MIDI {pad.midiNote}</span>
      </button>)}</section>
    <footer><span>SELECT A PAD, THEN LOAD, DROP OR AUDITION</span><span>1–4 VOICES PER PAD</span></footer>
  </main>
}
