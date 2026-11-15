import { useEffect, useRef, useState, type PointerEvent } from 'react'
import { readWaveform, setCoarseTune, setFineTune, setPadEdit, type Snapshot, type Waveform } from './bridge'

type EditDraft = { startFrame: number; endFrame: number; fadeInFrames: number; fadeOutFrames: number; reversed: boolean }
type Marker = 'start' | 'end' | 'fadeIn' | 'fadeOut'
type FrameField = 'startFrame' | 'endFrame' | 'fadeInFrames' | 'fadeOutFrames'

const fromSnapshot = (snapshot: Snapshot): EditDraft => ({
  startFrame: snapshot.trimStart, endFrame: snapshot.trimEnd,
  fadeInFrames: snapshot.fadeInFrames, fadeOutFrames: snapshot.fadeOutFrames,
  reversed: snapshot.reverse
})
const sameEdit = (left: EditDraft, right: EditDraft) =>
  left.startFrame === right.startFrame && left.endFrame === right.endFrame
  && left.fadeInFrames === right.fadeInFrames && left.fadeOutFrames === right.fadeOutFrames
  && left.reversed === right.reversed

export default function WaveformEditor({ snapshot }: { snapshot: Snapshot }) {
  const canvas = useRef<HTMLCanvasElement>(null)
  const cursorCanvas = useRef<HTMLCanvasElement>(null)
  const dragging = useRef<Marker | null>(null)
  const dragOrigin = useRef<{ edit: EditDraft; revision: number; pointerId: number } | null>(null)
  const summaries = useRef(new Map<string, Waveform>())
  const [draft, setDraft] = useState<EditDraft>(() => fromSnapshot(snapshot))
  const [frameEntries, setFrameEntries] = useState(() => {
    const initial = fromSnapshot(snapshot)
    return { startFrame: String(initial.startFrame), endFrame: String(initial.endFrame),
      fadeInFrames: String(initial.fadeInFrames), fadeOutFrames: String(initial.fadeOutFrames) }
  })
  const focusedFrame = useRef<FrameField | null>(null)
  const editRef = useRef<EditDraft>(fromSnapshot(snapshot))
  const pendingEdit = useRef<EditDraft | null>(null)
  const awaitingSnapshot = useRef<EditDraft | null>(null)
  const sending = useRef(false)
  const revision = useRef(snapshot.sampleRevision)
  const snapshotRef = useRef(snapshot)
  const cursorAnchor = useRef<{ frame: number; speed: number; time: number } | null>(null)
  const [waveform, setWaveform] = useState<Waveform | null>(null)
  const [canvasWidth, setCanvasWidth] = useState(0)
  const [zoom, setZoom] = useState(1)
  const [view, setView] = useState(0)
  const [error, setError] = useState('')
  const [coarse, setCoarse] = useState(snapshot.coarseTune)
  const [fine, setFine] = useState(snapshot.fineTune)
  const [coarseText, setCoarseText] = useState(String(snapshot.coarseTune))
  const [fineText, setFineText] = useState(String(snapshot.fineTune))
  const focusedTune = useRef<'coarse' | 'fine' | null>(null)
  const tuneValues = useRef({ coarse: snapshot.coarseTune, fine: snapshot.fineTune })
  const pendingTune = useRef<{ coarse?: number; fine?: number }>({})
  const awaitingTune = useRef<{ coarse?: number; fine?: number }>({})
  const sendingTune = useRef(false)
  const frames = snapshot.sourceFrames
  const rate = snapshot.sampleRate || 44100

  snapshotRef.current = snapshot
  useEffect(() => {
    const incoming = fromSnapshot(snapshot)
    if (revision.current !== snapshot.sampleRevision) {
      revision.current = snapshot.sampleRevision
      dragging.current = null
      dragOrigin.current = null
      pendingEdit.current = null
      awaitingSnapshot.current = null
      editRef.current = incoming
      setDraft(incoming)
      setError('')
      return
    }
    if (awaitingSnapshot.current) {
      if (!sameEdit(incoming, awaitingSnapshot.current)) return
      awaitingSnapshot.current = null
    }
    if (!dragging.current && !sending.current && !pendingEdit.current) {
      editRef.current = incoming
      setDraft(incoming)
    }
  }, [snapshot.sampleRevision, snapshot.trimStart, snapshot.trimEnd,
      snapshot.fadeInFrames, snapshot.fadeOutFrames, snapshot.reverse])

  useEffect(() => {
    if (awaitingTune.current.coarse === undefined
        || Math.abs(awaitingTune.current.coarse - snapshot.coarseTune) < 0.01) {
      delete awaitingTune.current.coarse
      tuneValues.current.coarse = snapshot.coarseTune
      setCoarse(snapshot.coarseTune)
      if (focusedTune.current !== 'coarse') setCoarseText(String(snapshot.coarseTune))
    }
    if (awaitingTune.current.fine === undefined
        || Math.abs(awaitingTune.current.fine - snapshot.fineTune) < 0.11) {
      delete awaitingTune.current.fine
      tuneValues.current.fine = snapshot.fineTune
      setFine(snapshot.fineTune)
      if (focusedTune.current !== 'fine') setFineText(String(snapshot.fineTune))
    }
  }, [snapshot.coarseTune, snapshot.fineTune])

  useEffect(() => {
    setFrameEntries(current => ({
      startFrame: focusedFrame.current === 'startFrame' ? current.startFrame : String(draft.startFrame),
      endFrame: focusedFrame.current === 'endFrame' ? current.endFrame : String(draft.endFrame),
      fadeInFrames: focusedFrame.current === 'fadeInFrames' ? current.fadeInFrames : String(draft.fadeInFrames),
      fadeOutFrames: focusedFrame.current === 'fadeOutFrames' ? current.fadeOutFrames : String(draft.fadeOutFrames)
    }))
  }, [draft])

  useEffect(() => {
    let alive = true
    if (frames > 0) {
      const level = zoom >= 4 ? 3 : 2
      const key = `${snapshot.selectedPad}:${snapshot.sampleRevision}:${level}`
      const cached = summaries.current.get(key)
      if (cached) setWaveform(cached)
      else {
        setWaveform(null)
        void readWaveform(snapshot.selectedPad, level).then(next => {
          if (!alive || next?.sampleRevision !== snapshot.sampleRevision) return
          summaries.current.set(key, next)
          if (summaries.current.size > 8)
            summaries.current.delete(summaries.current.keys().next().value!)
          setWaveform(next)
        }).catch(() => { if (alive) setError('WAVEFORM OVERVIEW UNAVAILABLE') })
      }
    } else setWaveform(null)
    return () => { alive = false }
  }, [snapshot.selectedPad, snapshot.sampleRevision, frames, zoom])

  const visibleFrames = Math.max(1, frames / zoom)
  const visibleStart = (frames - visibleFrames) * view
  const fadeInMarker = draft.reversed ? draft.endFrame - draft.fadeInFrames
    : draft.startFrame + draft.fadeInFrames
  const fadeOutMarker = draft.reversed ? draft.startFrame + draft.fadeOutFrames
    : draft.endFrame - draft.fadeOutFrames
  const toPixel = (frame: number, width: number) => (frame - visibleStart) / visibleFrames * width
  const toFrame = (x: number, width: number) => Math.round(Math.max(0,
    Math.min(frames, visibleStart + x / width * visibleFrames)))

  const flushEdits = async () => {
    if (sending.current) return
    sending.current = true
    while (pendingEdit.current) {
      const next = pendingEdit.current
      const sentRevision = revision.current
      pendingEdit.current = null
      let accepted = false
      try { accepted = await setPadEdit(snapshot.selectedPad, sentRevision, next) } catch { /* reported below */ }
      if (!accepted && sentRevision === revision.current) {
        pendingEdit.current = null
        awaitingSnapshot.current = null
        const current = fromSnapshot(snapshotRef.current)
        editRef.current = current
        setDraft(current)
        setError('EDIT COULD NOT BE APPLIED')
      }
    }
    sending.current = false
  }
  const changeEdit = (change: (current: EditDraft) => EditDraft) => {
    if (!frames) return
    const candidate = change(editRef.current)
    const startFrame = Math.max(0, Math.min(frames - 1, Math.round(candidate.startFrame)))
    const endFrame = Math.max(startFrame + 1, Math.min(frames, Math.round(candidate.endFrame)))
    const length = endFrame - startFrame
    const next: EditDraft = {
      startFrame, endFrame,
      fadeInFrames: Math.max(0, Math.min(length, Math.round(candidate.fadeInFrames))),
      fadeOutFrames: Math.max(0, Math.min(length, Math.round(candidate.fadeOutFrames))),
      reversed: candidate.reversed
    }
    if (sameEdit(next, editRef.current)) return
    editRef.current = next
    setDraft(next)
    setError('')
    awaitingSnapshot.current = next
    pendingEdit.current = next
    void flushEdits()
  }
  const cancelDrag = () => {
    const origin = dragOrigin.current
    if (!origin) return
    dragOrigin.current = null
    dragging.current = null
    if (origin.revision === revision.current)
      changeEdit(() => origin.edit)
    if (canvas.current?.hasPointerCapture(origin.pointerId))
      canvas.current.releasePointerCapture(origin.pointerId)
  }
  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape' && dragOrigin.current) {
        event.preventDefault()
        cancelDrag()
      }
    }
    window.addEventListener('keydown', onKeyDown)
    return () => window.removeEventListener('keydown', onKeyDown)
  })
  const flushTune = async () => {
    if (sendingTune.current) return
    sendingTune.current = true
    while (pendingTune.current.coarse !== undefined || pendingTune.current.fine !== undefined) {
      const kind = pendingTune.current.coarse !== undefined ? 'coarse' : 'fine'
      const value = pendingTune.current[kind]!
      delete pendingTune.current[kind]
      let accepted = false
      try {
        accepted = kind === 'coarse'
          ? await setCoarseTune(snapshot.selectedPad, value)
          : await setFineTune(snapshot.selectedPad, value)
      } catch { /* reported below */ }
      if (!accepted) {
        delete awaitingTune.current[kind]
        const restored = kind === 'coarse' ? snapshotRef.current.coarseTune : snapshotRef.current.fineTune
        tuneValues.current[kind] = restored
        if (kind === 'coarse') { setCoarse(restored); setCoarseText(String(restored)) }
        else { setFine(restored); setFineText(String(restored)) }
        setError('TUNING CHANGE COULD NOT BE APPLIED')
      }
    }
    sendingTune.current = false
  }
  const changeTune = (kind: 'coarse' | 'fine', value: number) => {
    tuneValues.current[kind] = value
    awaitingTune.current[kind] = value
    pendingTune.current[kind] = value
    if (kind === 'coarse') setCoarse(value)
    else setFine(value)
    setError('')
    void flushTune()
  }
  const typeTune = (kind: 'coarse' | 'fine', text: string) => {
    if (kind === 'coarse') setCoarseText(text)
    else setFineText(text)
    const numeric = kind === 'coarse' ? /^-?\d+$/.test(text)
      : /^-?(?:\d+\.?\d*|\.\d+)$/.test(text)
    if (!numeric) return
    const value = Number(text)
    if (kind === 'coarse' ? value >= -24 && value <= 24
      : Number.isFinite(value) && value >= -100 && value <= 100)
      changeTune(kind, value)
  }
  const finishTune = (kind: 'coarse' | 'fine') => {
    focusedTune.current = null
    const text = kind === 'coarse' ? coarseText : fineText
    if (!Number.isFinite(Number(text)) || text.trim() === ''
        || Math.abs(Number(text) - tuneValues.current[kind]) > 0.11)
      setError(kind === 'coarse' ? 'COARSE TUNE MUST BE -24 TO +24 SEMITONES'
        : 'FINE TUNE MUST BE -100 TO +100 CENTS')
    if (kind === 'coarse') setCoarseText(String(tuneValues.current.coarse))
    else setFineText(String(tuneValues.current.fine))
  }
  const typeFrame = (field: FrameField, text: string) => {
    setFrameEntries(current => ({ ...current, [field]: text }))
    if (!/^\d+$/.test(text)) return
    const value = Number(text)
    const current = editRef.current
    const length = current.endFrame - current.startFrame
    const valid = Number.isSafeInteger(value) && (field === 'startFrame'
      ? value >= 0 && value < current.endFrame
      : field === 'endFrame' ? value > current.startFrame && value <= frames
      : value >= 0 && value <= length)
    if (valid) changeEdit(edit => ({ ...edit, [field]: value }))
  }
  const finishFrameEntry = (field: FrameField) => {
    focusedFrame.current = null
    const text = frameEntries[field]
    if (!/^\d+$/.test(text) || Number(text) !== editRef.current[field])
      setError('ENTER A VALID FRAME VALUE')
    setFrameEntries(current => ({ ...current, [field]: String(editRef.current[field]) }))
  }

  useEffect(() => {
    const element = canvas.current
    if (!element) return
    const observer = new ResizeObserver(() => setCanvasWidth(element.clientWidth))
    observer.observe(element)
    setCanvasWidth(element.clientWidth)
    return () => observer.disconnect()
  }, [frames > 0])

  useEffect(() => {
    cursorAnchor.current = snapshot.playbackFrame < 0 ? null : {
      frame: snapshot.playbackFrame, speed: snapshot.playbackSpeed,
      time: performance.now()
    }
  }, [snapshot.playbackFrame, snapshot.playbackSpeed])

  useEffect(() => {
    const element = cursorCanvas.current
    if (!element) return
    const ratio = window.devicePixelRatio || 1
    const width = Math.max(1, element.clientWidth)
    const height = 156
    element.width = Math.round(width * ratio)
    element.height = Math.round(height * ratio)
    const context = element.getContext('2d')
    if (!context) return
    context.scale(ratio, ratio)
    let animation = 0
    const paint = (time: number) => {
      context.clearRect(0, 0, width, height)
      const position = cursorAnchor.current
      if (position) {
        const elapsed = Math.max(0, Math.min(100, time - position.time)) / 1000
        const frame = position.frame + position.speed * elapsed
        const x = toPixel(frame, width)
        if (x >= 0 && x <= width) {
          context.strokeStyle = '#f6efc4'
          context.lineWidth = 2
          context.beginPath()
          context.moveTo(x, 0)
          context.lineTo(x, height)
          context.stroke()
        }
      }
      animation = requestAnimationFrame(paint)
    }
    if (snapshot.playbackFrame >= 0) animation = requestAnimationFrame(paint)
    else context.clearRect(0, 0, width, height)
    return () => cancelAnimationFrame(animation)
  }, [snapshot.playbackFrame >= 0, frames, zoom, view, canvasWidth])

  useEffect(() => {
    const element = canvas.current
    if (!element) return
    const ratio = window.devicePixelRatio || 1
    const width = Math.max(1, element.clientWidth)
    const height = 160
    element.width = Math.round(width * ratio)
    element.height = Math.round(height * ratio)
    const context = element.getContext('2d')
    if (!context) return
    context.scale(ratio, ratio)
    context.fillStyle = '#101611'
    context.fillRect(0, 0, width, height)
    context.strokeStyle = '#34483a'
    context.lineWidth = 1
    for (const fraction of [0.25, 0.5, 0.75]) {
      context.beginPath()
      context.moveTo(0, height * fraction)
      context.lineTo(width, height * fraction)
      context.stroke()
    }
    if (!waveform || !frames) return
    const size = waveform.minimum.length
    const startX = toPixel(draft.startFrame, width)
    const endX = toPixel(draft.endFrame, width)
    context.fillStyle = '#090d0ab8'
    context.fillRect(0, 0, Math.max(0, startX), height)
    context.fillRect(Math.min(width, endX), 0, Math.max(0, width - endX), height)
    const regionFrames = draft.endFrame - draft.startFrame
    const strokePeak = (x: number, low: number, high: number) => {
      context.beginPath()
      context.moveTo(x + 0.5, height * (0.5 - Math.max(-1, Math.min(1, high)) * 0.42))
      context.lineTo(x + 0.5, height * (0.5 - Math.max(-1, Math.min(1, low)) * 0.42))
      context.stroke()
    }
    for (let x = 0; x < width; ++x) {
      const sourceFrame = visibleStart + (x + 0.5) / width * visibleFrames
      const first = Math.min(size - 1, Math.floor((visibleStart + x / width * visibleFrames) / frames * size))
      const last = Math.min(size, Math.max(first + 1,
        Math.ceil((visibleStart + (x + 1) / width * visibleFrames) / frames * size)))
      let low = 1, high = -1
      for (let bucket = first; bucket < last; ++bucket) {
        low = Math.min(low, waveform.minimum[bucket])
        high = Math.max(high, waveform.maximum[bucket])
      }
      context.strokeStyle = '#49614c'
      strokePeak(x, low, high)
      if (sourceFrame < draft.startFrame || sourceFrame >= draft.endFrame) continue
      const playbackPosition = draft.reversed
        ? draft.endFrame - 1 - sourceFrame : sourceFrame - draft.startFrame
      const fadeIn = draft.fadeInFrames > 0
        ? Math.max(0, Math.min(1, playbackPosition / draft.fadeInFrames)) : 1
      const fadeOut = draft.fadeOutFrames > 0
        ? Math.max(0, Math.min(1, (regionFrames - 1 - playbackPosition) / draft.fadeOutFrames)) : 1
      const editGain = fadeIn * fadeOut
      context.strokeStyle = '#a8d4aa'
      strokePeak(x, low * editGain, high * editGain)
    }
    const marker = (frame: number, color: string) => {
      const x = toPixel(frame, width)
      if (x < 0 || x > width) return
      context.strokeStyle = color
      context.beginPath()
      context.moveTo(x, 0)
      context.lineTo(x, height)
      context.stroke()
    }
    marker(draft.startFrame, '#f8bf73')
    marker(draft.endFrame, '#f8bf73')
    const fadeLine = (baseFrame: number, topFrame: number) => {
      const baseX = toPixel(baseFrame, width)
      const topX = toPixel(topFrame, width)
      context.strokeStyle = '#c4a0db'
      context.lineWidth = 2
      context.beginPath()
      context.moveTo(baseX, height)
      context.lineTo(topX, 0)
      context.stroke()
      context.lineWidth = 1
    }
    fadeLine(draft.reversed ? draft.endFrame : draft.startFrame, fadeInMarker)
    fadeLine(draft.reversed ? draft.startFrame : draft.endFrame, fadeOutMarker)
    const handle = (frame: number, y: number, color: string) => {
      const x = toPixel(frame, width)
      if (x < -7 || x > width + 7) return
      context.fillStyle = color
      context.fillRect(x - 5, y, 10, 10)
    }
    handle(draft.startFrame, height / 2 - 5, '#f8bf73')
    handle(draft.endFrame, height / 2 - 5, '#f8bf73')
    handle(fadeInMarker, 0, '#c4a0db')
    handle(fadeOutMarker, 0, '#c4a0db')
    context.fillStyle = '#83998a'
    context.font = '10px Consolas, monospace'
    context.fillText('FADE TIPS', 10, 34)
    context.fillText('TRIM', 10, height - 12)
    context.fillStyle = '#f7ca81'
    context.font = '11px Consolas, monospace'
    context.fillText(draft.reversed ? 'REVERSE' : 'FORWARD', 10, 17)
  }, [waveform, frames, draft, zoom, view, canvasWidth, fadeInMarker, fadeOutMarker])

  const moveMarker = (marker: Marker, frame: number) => changeEdit(current => {
    const next = { ...current }
    if (marker === 'start') next.startFrame = Math.min(frame, current.endFrame - 1)
    if (marker === 'end') next.endFrame = Math.max(frame, current.startFrame + 1)
    if (marker === 'fadeIn') next.fadeInFrames = Math.max(0,
      current.reversed ? current.endFrame - frame : frame - current.startFrame)
    if (marker === 'fadeOut') next.fadeOutFrames = Math.max(0,
      current.reversed ? frame - current.startFrame : current.endFrame - frame)
    const length = next.endFrame - next.startFrame
    next.fadeInFrames = Math.min(next.fadeInFrames, length)
    next.fadeOutFrames = Math.min(next.fadeOutFrames, length)
    return next
  })
  const positionFromPointer = (event: PointerEvent<HTMLCanvasElement>) => {
    const bounds = event.currentTarget.getBoundingClientRect()
    return toFrame(event.clientX - bounds.left, bounds.width)
  }
  const pointerDown = (event: PointerEvent<HTMLCanvasElement>) => {
    if (!frames) return
    if (dragOrigin.current) return
    const bounds = event.currentTarget.getBoundingClientRect()
    const x = event.clientX - bounds.left
    const fadeTipLane = event.clientY - bounds.top < bounds.height / 3
    const positions: [Marker, number][] = fadeTipLane
      ? [['fadeIn', fadeInMarker], ['fadeOut', fadeOutMarker]]
      : [['start', draft.startFrame], ['end', draft.endFrame]]
    positions.sort((left, right) => Math.abs(toPixel(left[1], bounds.width) - x)
      - Math.abs(toPixel(right[1], bounds.width) - x))
    dragOrigin.current = { edit: { ...editRef.current }, revision: revision.current,
      pointerId: event.pointerId }
    dragging.current = positions[0][0]
    event.currentTarget.setPointerCapture(event.pointerId)
    moveMarker(dragging.current, positionFromPointer(event))
  }
  const finishDrag = (event: PointerEvent<HTMLCanvasElement>) => {
    dragging.current = null
    dragOrigin.current = null
    if (event.currentTarget.hasPointerCapture(event.pointerId))
      event.currentTarget.releasePointerCapture(event.pointerId)
  }

  if (!frames) return <p className="wave-empty">LOAD A SAMPLE TO EDIT ITS WAVEFORM</p>
  const seconds = (frame: number) => (frame / rate).toFixed(4)
  const snapTune = () => {
    const snapped = Math.max(-24, Math.min(24, Math.round(
      tuneValues.current.coarse + tuneValues.current.fine / 100)))
    setCoarseText(String(snapped))
    setFineText('0')
    changeTune('coarse', snapped)
    changeTune('fine', 0)
  }
  const valid = draft.startFrame >= 0 && draft.endFrame <= frames
    && draft.startFrame < draft.endFrame && draft.fadeInFrames >= 0 && draft.fadeOutFrames >= 0
    && draft.fadeInFrames <= draft.endFrame - draft.startFrame
    && draft.fadeOutFrames <= draft.endFrame - draft.startFrame
  const tuneSemitones = coarse + fine / 100
  const playbackSeconds = valid && Number.isFinite(tuneSemitones)
    ? ((draft.endFrame - draft.startFrame) / rate / Math.pow(2, tuneSemitones / 12)).toFixed(4)
    : '--'
  return <div className="wave-editor">
    <div className="wave-canvas-wrap">
      <canvas ref={canvas} className="wave-canvas" aria-label="Sample waveform: drag fade tips at the top or trim markers below; edits apply immediately"
        onPointerDown={pointerDown}
        onPointerMove={event => { if (dragging.current) moveMarker(dragging.current, positionFromPointer(event)) }}
        onPointerUp={finishDrag} onPointerCancel={cancelDrag}
        onLostPointerCapture={cancelDrag} />
      <canvas ref={cursorCanvas} className="wave-cursor" aria-hidden="true" />
    </div>
    <div className="wave-readout"><span>START {seconds(draft.startFrame)} S</span><span>END {seconds(draft.endFrame)} S</span><span>{frames} SOURCE FRAMES</span></div>
    <div className="wave-readout"><span>PLAYBACK ~{playbackSeconds} S</span><span>RAW TRACE DIM / EDIT PREVIEW BRIGHT</span></div>
    <div className="wave-controls">
      <label>START <input type="number" min="0" max={frames - 1} value={frameEntries.startFrame}
        onFocus={() => { focusedFrame.current = 'startFrame' }} onBlur={() => finishFrameEntry('startFrame')}
        onChange={event => typeFrame('startFrame', event.target.value)} /></label>
      <label>END <input type="number" min="1" max={frames} value={frameEntries.endFrame}
        onFocus={() => { focusedFrame.current = 'endFrame' }} onBlur={() => finishFrameEntry('endFrame')}
        onChange={event => typeFrame('endFrame', event.target.value)} /></label>
      <label>FADE IN <input type="number" min="0" max={frames} value={frameEntries.fadeInFrames}
        onFocus={() => { focusedFrame.current = 'fadeInFrames' }} onBlur={() => finishFrameEntry('fadeInFrames')}
        onChange={event => typeFrame('fadeInFrames', event.target.value)} /></label>
      <label>FADE OUT <input type="number" min="0" max={frames} value={frameEntries.fadeOutFrames}
        onFocus={() => { focusedFrame.current = 'fadeOutFrames' }} onBlur={() => finishFrameEntry('fadeOutFrames')}
        onChange={event => typeFrame('fadeOutFrames', event.target.value)} /></label>
      <label><input type="checkbox" checked={draft.reversed}
        onChange={event => changeEdit(current => ({ ...current, reversed: event.target.checked }))} /> REVERSE</label>
      <label>ZOOM <select value={zoom} onChange={event => { setZoom(Number(event.target.value)); setView(0) }}>
        {[1, 2, 4, 8].map(value => <option key={value} value={value}>{value}×</option>)}
      </select></label>
      {zoom > 1 && <label>VIEW <input type="range" min="0" max="1" step="0.001" value={view}
        onChange={event => setView(Number(event.target.value))} /></label>}
    </div>
    <div className="wave-controls" aria-label="Pad tuning">
      <label>COARSE SEMITONES <input type="number" min="-24" max="24" step="1" value={coarseText}
        onFocus={() => { focusedTune.current = 'coarse' }} onBlur={() => finishTune('coarse')}
        onChange={event => typeTune('coarse', event.target.value)} /></label>
      <label>FINE CENTS <input type="number" min="-100" max="100" step="0.1" value={fineText}
        onFocus={() => { focusedTune.current = 'fine' }} onBlur={() => finishTune('fine')}
        onChange={event => typeTune('fine', event.target.value)} /></label>
      <button onClick={snapTune}>SNAP TO SEMITONE</button>
    </div>
    {error && <p className="operation-error" role="alert">{error}</p>}
  </div>
}
