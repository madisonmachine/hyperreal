import { useEffect, useRef, useState } from 'react'
import { audition, cancelLoad, chooseSample, clearPad, copyPad, importDroppedFile, pastePad, readSnapshot, selectPad, setChokeGroup, setMasterGain, setPadMode, setPadMute, setPadSolo, setVelocityCurve, setVoiceLimit, swapPads, type Snapshot } from './bridge'
import WaveformEditor from './WaveformEditor'

const initial: Snapshot = {
  schemaVersion: 2, selectedPad: 0,
  pads: Array.from({ length: 16 }, (_, index) => ({
    index, midiNote: index + 36, midiTriggerCount: 0, status: 'empty', sampleName: '', muted: false, solo: false
  })),
  sampleName: '', status: 'empty', message: '', durationSeconds: 0,
  sampleRate: 0, sourceFrames: 0, sampleRevision: 0,
  trimStart: 0, trimEnd: 0, fadeInFrames: 0, fadeOutFrames: 0,
  reverse: false, playbackFrame: -1,
  masterGain: 1, padLevel: 1, padMode: 0, chokeGroup: 0,
  muted: false, solo: false, clipboardReady: false,
  voiceLimit: 4, velocityCurve: 1, coarseTune: 0, fineTune: 0
}

export default function App() {
  const [snapshot, setSnapshot] = useState<Snapshot>(initial)
  const [swapTarget, setSwapTarget] = useState(1)
  const [pending, setPending] = useState<{ kind: 'paste'|'swap'|'clear'; pad: number; otherPad?: number } | null>(null)
  const [context, setContext] = useState<{ pad: number; x: number; y: number } | null>(null)
  const [dropProgress, setDropProgress] = useState<{ pad: number; percent: number } | null>(null)
  const [waveformOpen, setWaveformOpen] = useState(false)
  const [auditionFlash, setAuditionFlash] = useState<number | null>(null)
  const auditionTimer = useRef<number | undefined>(undefined)
  const [operationError, setOperationError] = useState('')
  useEffect(() => {
    let alive = true
    let lastMidiCounts: number[] | null = null
    const midiFlashUntil = Array<number>(16).fill(0)
    const refresh = async () => {
      try {
        const next = await readSnapshot()
        if (alive) {
          const now = Date.now()
          const counts = next.pads.map(pad => pad.midiTriggerCount)
          if (lastMidiCounts)
            counts.forEach((count, index) => {
              if (count !== lastMidiCounts?.[index]) midiFlashUntil[index] = now + 350
            })
          lastMidiCounts = counts
          setSnapshot({ ...next, pads: next.pads.map((pad, index) => ({
            ...pad, midiActive: now < midiFlashUntil[index]
          })) })
        }
      } catch {
        if (alive) setSnapshot(current => ({
          ...current, status: 'error', message: 'Native state unavailable.'
        }))
      }
    }
    void refresh()
    const timer = window.setInterval(() => void refresh(), 250)
    return () => { alive = false; window.clearInterval(timer); window.clearTimeout(auditionTimer.current) }
  }, [])

  const selected = snapshot.selectedPad
  const selectedLabel = String(selected + 1).padStart(2, '0')
  const handleDroppedFile = (pad: number, file: File | undefined) => {
    if (!file) return
    setContext(null)
    setOperationError('')
    setDropProgress({ pad, percent: 0 })
    void importDroppedFile(pad, file, percent => setDropProgress({ pad, percent }))
      .catch(error => setOperationError(error instanceof Error ? error.message : 'SAMPLE DROP FAILED'))
      .finally(() => setDropProgress(null))
  }
  const confirmOperation = async () => {
    if (!pending) return
    const success = pending.kind === 'paste' ? await pastePad(pending.pad)
      : pending.kind === 'swap' ? await swapPads(pending.pad, pending.otherPad ?? pending.pad)
      : await clearPad(pending.pad)
    setOperationError(success ? '' : 'PAD OPERATION FAILED. CHECK THE SAMPLE MEMORY LIMIT.')
    setPending(null)
    setContext(null)
  }
  return <main className="chassis"
    onDragOver={event => { if (event.dataTransfer.types.includes('Files')) event.preventDefault() }}
    onDrop={event => { event.preventDefault(); handleDroppedFile(selected, event.dataTransfer.files[0]) }}>
    <header><div><strong>HYPERREAL</strong><small>16 PAD DRUM SAMPLER</small></div><span>PHASE 4 DEVELOPMENT</span></header>
    <section className="display" aria-live="polite">
      <div className="display-row"><span>PAD {selectedLabel} / MIDI {selected + 36}</span><span>{snapshot.status.toUpperCase()}</span></div>
      <h1>{snapshot.sampleName || 'NO SAMPLE LOADED'}</h1>
      <div className="display-row"><span>{snapshot.sampleRate ? (snapshot.sampleRate/1000).toFixed(1)+' KHZ' : '-- KHZ'}</span><span>{snapshot.durationSeconds ? snapshot.durationSeconds.toFixed(2)+' SEC' : '-- SEC'}</span></div>
      <p>{snapshot.message || 'RIGHT-CLICK A PAD TO LOAD A SAMPLE, OR DROP A FILE ON IT'}</p>
      {snapshot.status === 'loading' && <button className="cancel-load" onClick={() => void cancelLoad(selected)}>CANCEL LOAD</button>}
    </section>
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
    {operationError && <p className="operation-error" role="alert">{operationError}</p>}
    <section className="pads" aria-label="Sixteen drum pads">{snapshot.pads.map((pad) =>
      <button key={pad.midiNote}
        data-pad-index={pad.index}
        className={'pad '+(pad.index === selected ? 'selected ' : '')+(pad.status === 'ready' ? 'loaded ' : '')+(pad.muted ? 'muted ' : '')+(pad.solo ? 'solo ' : '')+(pad.midiActive ? 'midi-trigger ' : '')+(auditionFlash === pad.index ? 'audition-trigger' : '')}
        onClick={() => { setContext(null); setPending(null); setOperationError(''); void selectPad(pad.index); if (pad.status === 'ready') { void audition(pad.index); setAuditionFlash(pad.index); window.clearTimeout(auditionTimer.current); auditionTimer.current = window.setTimeout(() => setAuditionFlash(null), 180) } }}
        onContextMenu={event => { event.preventDefault(); setPending(null); setContext({ pad: pad.index, x: event.clientX, y: event.clientY }); void selectPad(pad.index) }}
        onDragOver={event => { if (event.dataTransfer.types.includes('Files')) { event.preventDefault(); event.dataTransfer.dropEffect = 'copy' } }}
        onDrop={event => { event.preventDefault(); event.stopPropagation(); handleDroppedFile(pad.index, event.dataTransfer.files[0]) }}
        aria-label={'Pad '+(pad.index+1)+', MIDI note '+pad.midiNote+', '+pad.status+(pad.muted ? ', muted' : '')+(pad.solo ? ', solo' : '')+'; left-click to audition, right-click for options'}
        aria-pressed={pad.index === selected}>
        <span>{String(pad.index+1).padStart(2,'0')} · {dropProgress?.pad === pad.index ? 'IMPORT '+dropProgress.percent+'%' : pad.midiActive ? 'MIDI' : auditionFlash === pad.index ? 'AUDITION' : pad.muted ? 'MUTED' : pad.solo ? 'SOLO' : pad.status.toUpperCase()}</span>
        <b>{pad.sampleName || 'UNASSIGNED'}</b>
        <span>MIDI {pad.midiNote}</span>
      </button>)}</section>
    <details className="waveform-panel" onToggle={event => setWaveformOpen(event.currentTarget.open)}>
      <summary>WAVEFORM / SAMPLE EDIT</summary>{waveformOpen && <WaveformEditor snapshot={snapshot} />}
    </details>
    {context && <>
      <div className="context-dismiss" onClick={() => { setContext(null); setPending(null) }} />
      <div className="pad-context" role="menu" aria-label={'Pad '+(context.pad+1)+' options'} style={{ left: Math.min(context.x, window.innerWidth - 210), top: Math.min(context.y, window.innerHeight - 270) }}>
        <strong>PAD {String(context.pad+1).padStart(2,'0')}</strong>
        {pending ? <>
          <p>CONFIRM {pending.kind.toUpperCase()} PAD {String(pending.pad+1).padStart(2,'0')}{pending.otherPad !== undefined ? ' WITH '+String(pending.otherPad+1).padStart(2,'0') : ''}?</p>
          <button onClick={() => void confirmOperation()}>CONFIRM</button>
          <button onClick={() => setPending(null)}>CANCEL</button>
        </> : <>
          <button onClick={() => { void chooseSample(context.pad); setContext(null) }}>LOAD SAMPLE</button>
          <button onClick={() => { void copyPad(context.pad).then(success => setOperationError(success ? '' : 'COPY FAILED')); setContext(null) }}>COPY PAD</button>
          <button disabled={!snapshot.clipboardReady} onClick={() => setPending({ kind: 'paste', pad: context.pad })}>PASTE TO PAD</button>
          <label>SWAP WITH <select value={swapTarget} onChange={event => setSwapTarget(Number(event.target.value))}>{snapshot.pads.map(pad => <option key={pad.index} value={pad.index}>PAD {String(pad.index+1).padStart(2,'0')}</option>)}</select></label>
          <button disabled={swapTarget === context.pad} onClick={() => setPending({ kind: 'swap', pad: context.pad, otherPad: swapTarget })}>SWAP PADS</button>
          <button onClick={() => setPending({ kind: 'clear', pad: context.pad })}>CLEAR PAD</button>
        </>}
      </div>
    </>}
    <footer><span>LEFT-CLICK AUDITION · RIGHT-CLICK OPTIONS · DROP SAMPLE</span><span>1–4 VOICES PER PAD</span></footer>
  </main>
}
