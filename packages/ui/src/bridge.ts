import { getNativeFunction } from '@juce-framework/webview'

export type PadSnapshot = { index: number; midiNote: number; status: 'empty'|'loading'|'ready'|'error'; sampleName: string; muted: boolean; solo: boolean }
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
  muted: boolean
  solo: boolean
  clipboardReady: boolean
  voiceLimit: number
  velocityCurve: number
}
const demo: Snapshot = {
  schemaVersion: 2, selectedPad: 0,
  pads: Array.from({ length: 16 }, (_, index) => ({
    index, midiNote: index + 36, status: 'empty', sampleName: '', muted: false, solo: false
  })),
  sampleName: '', status: 'empty', message: 'Open the plugin to load a sample.',
  durationSeconds: 0, sampleRate: 0, masterGain: 1,
  padLevel: 1, padMode: 0, chokeGroup: 0, muted: false, solo: false,
  clipboardReady: false, voiceLimit: 4, velocityCurve: 1
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
