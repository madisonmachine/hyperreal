import { useEffect, useRef, useState, type PointerEvent } from 'react'
import { readWaveform, setCoarseTune, setFineTune, setPadEdit, type Snapshot, type Waveform } from './bridge'

type EditDraft = { startFrame: number; endFrame: number; fadeInFrames: number; fadeOutFrames: number; reversed: boolean }
type Marker = 'start' | 'end' | 'fadeIn' | 'fadeOut'

const fromSnapshot = (snapshot: Snapshot): EditDraft => ({
  startFrame: snapshot.trimStart, endFrame: snapshot.trimEnd,
  fadeInFrames: snapshot.fadeInFrames, fadeOutFrames: snapshot.fadeOutFrames,
  reversed: snapshot.reverse
})

export default function WaveformEditor({ snapshot }: { snapshot: Snapshot }) {
  const canvas = useRef<HTMLCanvasElement>(null)
  const dragging = useRef<Marker | null>(null)
  const [draft, setDraft] = useState<EditDraft>(() => fromSnapshot(snapshot))
  const [waveform, setWaveform] = useState<Waveform | null>(null)
  const [zoom, setZoom] = useState(1)
  const [view, setView] = useState(0)
  const [error, setError] = useState('')
  const [saving, setSaving] = useState(false)
  const [coarse, setCoarse] = useState(snapshot.coarseTune)
  const [fine, setFine] = useState(snapshot.fineTune)
  const frames = snapshot.sourceFrames
  const rate = snapshot.sampleRate || 44100

  useEffect(() => {
    setDraft(fromSnapshot(snapshot))
    setError('')
  }, [snapshot.selectedPad, snapshot.trimStart, snapshot.trimEnd,
      snapshot.fadeInFrames, snapshot.fadeOutFrames, snapshot.reverse])

  useEffect(() => { setCoarse(snapshot.coarseTune); setFine(snapshot.fineTune) },
    [snapshot.selectedPad, snapshot.coarseTune, snapshot.fineTune])

  useEffect(() => {
    let alive = true
    setWaveform(null)
    if (frames > 0) {
      const level = zoom >= 4 ? 3 : 2
      void readWaveform(snapshot.selectedPad, level).then(next => {
        if (alive && next?.sampleRevision === snapshot.sampleRevision) setWaveform(next)
      }).catch(() => { if (alive) setError('WAVEFORM OVERVIEW UNAVAILABLE') })
    }
    return () => { alive = false }
  }, [snapshot.selectedPad, snapshot.sampleRevision, frames, zoom])

  const visibleFrames = Math.max(1, frames / zoom)
  const visibleStart = (frames - visibleFrames) * view
  const toPixel = (frame: number, width: number) => (frame - visibleStart) / visibleFrames * width
  const toFrame = (x: number, width: number) => Math.round(Math.max(0,
    Math.min(frames, visibleStart + x / width * visibleFrames)))

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
    context.strokeStyle = '#a8d4aa'
    for (let x = 0; x < width; ++x) {
      const first = Math.min(size - 1, Math.floor((visibleStart + x / width * visibleFrames) / frames * size))
      const last = Math.min(size, Math.max(first + 1,
        Math.ceil((visibleStart + (x + 1) / width * visibleFrames) / frames * size)))
      let low = 1, high = -1
      for (let bucket = first; bucket < last; ++bucket) {
        low = Math.min(low, waveform.minimum[bucket])
        high = Math.max(high, waveform.maximum[bucket])
      }
      context.beginPath()
      context.moveTo(x + 0.5, height * (0.5 - Math.max(-1, Math.min(1, high)) * 0.42))
      context.lineTo(x + 0.5, height * (0.5 - Math.max(-1, Math.min(1, low)) * 0.42))
      context.stroke()
    }
    const startX = toPixel(draft.startFrame, width)
    const endX = toPixel(draft.endFrame, width)
    context.fillStyle = '#090d0ab8'
    context.fillRect(0, 0, Math.max(0, startX), height)
    context.fillRect(Math.min(width, endX), 0, Math.max(0, width - endX), height)
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
    marker(draft.startFrame + draft.fadeInFrames, '#9a78b0')
    marker(draft.endFrame - draft.fadeOutFrames, '#9a78b0')
    if (snapshot.playbackFrame >= 0) marker(snapshot.playbackFrame, '#e4efb6')
    context.fillStyle = '#f7ca81'
    context.font = '11px Consolas, monospace'
    context.fillText(draft.reversed ? 'REVERSE' : 'FORWARD', 10, 17)
  }, [waveform, frames, draft, zoom, view, snapshot.playbackFrame])

  const moveMarker = (marker: Marker, frame: number) => setDraft(current => {
    const next = { ...current }
    if (marker === 'start') next.startFrame = Math.min(frame, current.endFrame - 1)
    if (marker === 'end') next.endFrame = Math.max(frame, current.startFrame + 1)
    if (marker === 'fadeIn') next.fadeInFrames = Math.max(0, frame - current.startFrame)
    if (marker === 'fadeOut') next.fadeOutFrames = Math.max(0, current.endFrame - frame)
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
    const bounds = event.currentTarget.getBoundingClientRect()
    const x = event.clientX - bounds.left
    const positions: [Marker, number][] = [
      ['start', draft.startFrame], ['end', draft.endFrame],
      ['fadeIn', draft.startFrame + draft.fadeInFrames],
      ['fadeOut', draft.endFrame - draft.fadeOutFrames]
    ]
    positions.sort((left, right) => Math.abs(toPixel(left[1], bounds.width) - x)
      - Math.abs(toPixel(right[1], bounds.width) - x))
    dragging.current = positions[0][0]
    event.currentTarget.setPointerCapture(event.pointerId)
    moveMarker(dragging.current, positionFromPointer(event))
  }

  if (!frames) return <p className="wave-empty">LOAD A SAMPLE TO EDIT ITS WAVEFORM</p>
  const seconds = (frame: number) => (frame / rate).toFixed(4)
  const valid = draft.startFrame >= 0 && draft.endFrame <= frames
    && draft.startFrame < draft.endFrame && draft.fadeInFrames >= 0 && draft.fadeOutFrames >= 0
    && draft.fadeInFrames <= draft.endFrame - draft.startFrame
    && draft.fadeOutFrames <= draft.endFrame - draft.startFrame
  return <div className="wave-editor">
    <canvas ref={canvas} className="wave-canvas" aria-label="Sample waveform with trim and fade markers"
      onPointerDown={pointerDown}
      onPointerMove={event => { if (dragging.current) moveMarker(dragging.current, positionFromPointer(event)) }}
      onPointerUp={event => { dragging.current = null; event.currentTarget.releasePointerCapture(event.pointerId) }} />
    <div className="wave-readout"><span>START {seconds(draft.startFrame)} S</span><span>END {seconds(draft.endFrame)} S</span><span>{frames} SOURCE FRAMES</span></div>
    <div className="wave-controls">
      <label>START <input type="number" min="0" max={frames - 1} value={draft.startFrame}
        onChange={event => setDraft(current => ({ ...current, startFrame: Number(event.target.value) }))} /></label>
      <label>END <input type="number" min="1" max={frames} value={draft.endFrame}
        onChange={event => setDraft(current => ({ ...current, endFrame: Number(event.target.value) }))} /></label>
      <label>FADE IN <input type="number" min="0" max={frames} value={draft.fadeInFrames}
        onChange={event => setDraft(current => ({ ...current, fadeInFrames: Number(event.target.value) }))} /></label>
      <label>FADE OUT <input type="number" min="0" max={frames} value={draft.fadeOutFrames}
        onChange={event => setDraft(current => ({ ...current, fadeOutFrames: Number(event.target.value) }))} /></label>
      <label><input type="checkbox" checked={draft.reversed}
        onChange={event => setDraft(current => ({ ...current, reversed: event.target.checked }))} /> REVERSE</label>
      <label>ZOOM <select value={zoom} onChange={event => { setZoom(Number(event.target.value)); setView(0) }}>
        {[1, 2, 4, 8].map(value => <option key={value} value={value}>{value}×</option>)}
      </select></label>
      {zoom > 1 && <label>VIEW <input type="range" min="0" max="1" step="0.001" value={view}
        onChange={event => setView(Number(event.target.value))} /></label>}
    </div>
    <div className="wave-controls" aria-label="Pad tuning">
      <label>COARSE SEMITONES <input type="number" min="-24" max="24" step="1" value={coarse}
        onChange={event => setCoarse(Number(event.target.value))}
        onBlur={() => { if (Number.isInteger(coarse) && coarse >= -24 && coarse <= 24) void setCoarseTune(snapshot.selectedPad, coarse) }}
        onKeyDown={event => { if (event.key === 'Enter') event.currentTarget.blur() }} /></label>
      <label>FINE CENTS <input type="number" min="-100" max="100" step="0.1" value={fine}
        onChange={event => setFine(Number(event.target.value))}
        onBlur={() => { if (Number.isFinite(fine) && fine >= -100 && fine <= 100) void setFineTune(snapshot.selectedPad, fine) }}
        onKeyDown={event => { if (event.key === 'Enter') event.currentTarget.blur() }} /></label>
      <button onClick={() => { setFine(0); void setFineTune(snapshot.selectedPad, 0) }}>SNAP TO SEMITONE</button>
    </div>
    <div className="wave-actions">
      <button disabled={!valid || saving} onClick={() => {
        setSaving(true)
        void setPadEdit(snapshot.selectedPad, { ...draft,
          startFrame: Math.round(draft.startFrame), endFrame: Math.round(draft.endFrame),
          fadeInFrames: Math.round(draft.fadeInFrames), fadeOutFrames: Math.round(draft.fadeOutFrames)
        }).then(ok => setError(ok ? '' : 'EDIT REJECTED: CHECK REGION AND FADE VALUES'))
          .catch(() => setError('EDIT COULD NOT BE APPLIED'))
          .finally(() => setSaving(false))
      }}>APPLY EDIT</button>
      <button onClick={() => { setDraft(fromSnapshot(snapshot)); setError('') }}>REVERT DRAFT</button>
      <button onClick={() => setDraft({ startFrame: 0, endFrame: frames, fadeInFrames: 0,
        fadeOutFrames: Math.min(frames - 1, Math.round(rate * 0.005)), reversed: false })}>FULL SAMPLE</button>
    </div>
    {error && <p className="operation-error" role="alert">{error}</p>}
  </div>
}
