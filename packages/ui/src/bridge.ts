import { getNativeFunction } from '@juce-framework/webview'

export type PadSnapshot = { index: number; midiNote: number; status: 'empty'|'loading'|'ready'|'error'; sampleName: string }
export type Snapshot = {
  schemaVersion: number
  selectedPad: number
  pads: PadSnapshot[]
  sampleName: string
  status: 'empty'|'loading'|'ready'|'error'
  message: string
  durationSeconds: number
  sampleRate: number
  masterGain: number
  padLevel: number
  padMode: number
  chokeGroup: number
}
const demo: Snapshot = {
  schemaVersion: 2, selectedPad: 0,
  pads: Array.from({ length: 16 }, (_, index) => ({
    index, midiNote: index + 36, status: 'empty', sampleName: ''
  })),
  sampleName: '', status: 'empty', message: 'Open the plugin to load a sample.',
  durationSeconds: 0, sampleRate: 0, masterGain: 1,
  padLevel: 1, padMode: 0, chokeGroup: 0
}
const nativeAvailable = () => Boolean((window as Window & { __JUCE__?: unknown }).__JUCE__)
export async function readSnapshot(): Promise<Snapshot> {
  return nativeAvailable() ? await getNativeFunction('getSnapshot')() as Snapshot : demo
}
export async function chooseSample(): Promise<void> {
  if (nativeAvailable()) await getNativeFunction('chooseSample')()
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
