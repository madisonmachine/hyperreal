import { getNativeFunction } from '@juce-framework/webview'

export type PadSnapshot = { index: number; midiNote: number; midiTriggerCount: number; midiActive?: boolean; status: 'empty'|'loading'|'ready'|'error'; sampleName: string; muted: boolean; solo: boolean }
export type Snapshot = {
  schemaVersion: number
  selectedPad: number
  pads: PadSnapshot[]
  sampleName: string
  status: 'empty'|'loading'|'ready'|'error'
  message: string
  durationSeconds: number
  sampleRate: number
  sourceFrames: number
  sampleRevision: number
  trimStart: number
  trimEnd: number
  fadeInFrames: number
  fadeOutFrames: number
  reverse: boolean
  playbackFrame: number
  playbackSpeed: number
  masterGain: number
  padLevel: number
  padMode: number
  chokeGroup: number
  muted: boolean
  solo: boolean
  clipboardReady: boolean
  voiceLimit: number
  velocityCurve: number
  coarseTune: number
  fineTune: number
  ampAttack: number
  ampDecay: number
  ampSustain: number
  ampRelease: number
  filterMode: number
  filterCutoff: number
  filterResonance: number
  filterDepth: number
  filterAttack: number
  filterDecay: number
  filterSustain: number
  filterRelease: number
  lfoDestination: number
  lfoShape: number
  lfoRate: number
  lfoDepth: number
  lfoPhaseMode: number
}
export type Waveform = { schemaVersion: number; pad: number; level: number; sampleRevision: number; minimum: number[]; maximum: number[] }
const demo: Snapshot = {
  schemaVersion: 2, selectedPad: 0,
  pads: Array.from({ length: 16 }, (_, index) => ({
    index, midiNote: index + 36, midiTriggerCount: 0, status: 'empty', sampleName: '', muted: false, solo: false
  })),
  sampleName: '', status: 'empty', message: 'Open the plugin to load a sample.',
  durationSeconds: 0, sampleRate: 0, sourceFrames: 0, sampleRevision: 0,
  trimStart: 0, trimEnd: 0, fadeInFrames: 0, fadeOutFrames: 0,
  reverse: false, playbackFrame: -1, playbackSpeed: 0, masterGain: 1,
  padLevel: 1, padMode: 0, chokeGroup: 0, muted: false, solo: false,
  clipboardReady: false, voiceLimit: 4, velocityCurve: 1, coarseTune: 0, fineTune: 0,
  ampAttack: 0, ampDecay: 0, ampSustain: 1, ampRelease: 0,
  filterMode: 0, filterCutoff: 20000, filterResonance: 0.707, filterDepth: 0,
  filterAttack: 0, filterDecay: 0, filterSustain: 1, filterRelease: 0,
  lfoDestination: 0, lfoShape: 0, lfoRate: 1, lfoDepth: 0, lfoPhaseMode: 0
}
const nativeAvailable = () => Boolean((window as Window & { __JUCE__?: unknown }).__JUCE__)
export async function readSnapshot(): Promise<Snapshot> {
  return nativeAvailable() ? await getNativeFunction('getSnapshot')() as Snapshot : demo
}
export async function readWaveform(pad: number, level: number): Promise<Waveform | null> {
  return nativeAvailable() ? await getNativeFunction('getWaveform')({ schemaVersion: 6, pad, level }) as Waveform | null : null
}
export async function setPadEdit(pad: number, sampleRevision: number, edit: { startFrame: number; endFrame: number; fadeInFrames: number; fadeOutFrames: number; reversed: boolean }): Promise<boolean> {
  return nativeAvailable() ? Boolean(await getNativeFunction('setPadEdit')({ schemaVersion: 6, pad, sampleRevision, ...edit })) : false
}
export async function setCoarseTune(pad: number, value: number): Promise<boolean> {
  return nativeAvailable() ? Boolean(await getNativeFunction('setCoarseTune')({ schemaVersion: 3, pad, value })) : false
}
export async function setFineTune(pad: number, value: number): Promise<boolean> {
  return nativeAvailable() ? Boolean(await getNativeFunction('setFineTune')({ schemaVersion: 6, pad, value })) : false
}
export async function setPadAmplitude(pad: number, field: 'attack'|'decay'|'sustain'|'release', value: number): Promise<boolean> {
  return nativeAvailable() ? Boolean(await getNativeFunction('setPadAmplitude')({ schemaVersion: 8, pad, field, value })) : false
}
export async function setPadFilter(pad: number, field: 'mode'|'cutoff'|'resonance'|'depth'|'attack'|'decay'|'sustain'|'release', value: number): Promise<boolean> {
  return nativeAvailable() ? Boolean(await getNativeFunction('setPadFilter')({ schemaVersion: 9, pad, field, value })) : false
}
export async function setPadLfo(pad: number, field: 'destination'|'shape'|'rate'|'depth'|'phase', value: number): Promise<boolean> {
  return nativeAvailable() ? Boolean(await getNativeFunction('setPadLfo')({ schemaVersion: 10, pad, field, value })) : false
}
export async function chooseSample(pad: number): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('chooseSample')({ schemaVersion: 2, pad })
}
export async function importDroppedFile(pad: number, file: File, onProgress: (percent: number) => void): Promise<void> {
  if (!nativeAvailable()) throw new Error('Native sample import is unavailable.')
  const token = Number(await getNativeFunction('beginDrop')({ schemaVersion: 5, pad, name: file.name, size: file.size }))
  if (!Number.isInteger(token) || token < 1) throw new Error('Choose a WAV, AIFF or FLAC file under 128 MB.')
  try {
    const chunkSize = 65536
    for (let offset = 0; offset < file.size; offset += chunkSize) {
      const bytes = new Uint8Array(await file.slice(offset, Math.min(offset + chunkSize, file.size)).arrayBuffer())
      let binary = ''
      for (const byte of bytes) binary += String.fromCharCode(byte)
      const accepted = await getNativeFunction('appendDrop')({ schemaVersion: 5, token, chunk: btoa(binary) })
      if (!accepted) throw new Error('The dropped file transfer was interrupted.')
      onProgress(Math.round(100 * Math.min(offset + bytes.length, file.size) / file.size))
    }
    const finished = await getNativeFunction('finishDrop')({ schemaVersion: 5, token })
    if (!finished) throw new Error('The dropped file transfer was incomplete.')
  } catch (error) {
    await getNativeFunction('cancelDrop')({ schemaVersion: 5, token }).catch(() => {})
    throw error
  }
}
export async function selectPad(pad: number): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('selectPad')({ schemaVersion: 2, pad })
}
export async function cancelLoad(pad: number): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('cancelLoad')({ schemaVersion: 2, pad })
}
export async function audition(pad: number): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('audition')({ schemaVersion: 2, pad })
}
export async function setMasterGain(value: number): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('setMasterGain')({ schemaVersion: 1, value })
}
export async function setPadMode(pad: number, value: number): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('setPadMode')({ schemaVersion: 3, pad, value })
}
export async function setChokeGroup(pad: number, value: number): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('setChokeGroup')({ schemaVersion: 3, pad, value })
}
export async function setPadMute(pad: number, value: boolean): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('setPadMute')({ schemaVersion: 3, pad, value: Number(value) })
}
export async function setPadSolo(pad: number, value: boolean): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('setPadSolo')({ schemaVersion: 3, pad, value: Number(value) })
}
export async function setVoiceLimit(pad: number, value: number): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('setVoiceLimit')({ schemaVersion: 3, pad, value })
}
export async function setVelocityCurve(pad: number, value: number): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('setVelocityCurve')({ schemaVersion: 3, pad, value })
}
export async function copyPad(pad: number): Promise<boolean> {
  return nativeAvailable() ? Boolean(await getNativeFunction('copyPad')({ schemaVersion: 4, pad })) : false
}
export async function pastePad(pad: number): Promise<boolean> {
  return nativeAvailable() ? Boolean(await getNativeFunction('pastePad')({ schemaVersion: 4, pad, confirmed: true })) : false
}
export async function swapPads(pad: number, otherPad: number): Promise<boolean> {
  return nativeAvailable() ? Boolean(await getNativeFunction('swapPads')({ schemaVersion: 4, pad, otherPad, confirmed: true })) : false
}
export async function clearPad(pad: number): Promise<boolean> {
  return nativeAvailable() ? Boolean(await getNativeFunction('clearPad')({ schemaVersion: 4, pad, confirmed: true })) : false
}
