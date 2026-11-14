import { getNativeFunction } from '@juce-framework/webview'
export type Snapshot = { schemaVersion: number; sampleName: string; status: 'empty'|'loading'|'ready'|'error'; message: string; durationSeconds: number; sampleRate: number; masterGain: number }
const demo: Snapshot = { schemaVersion: 1, sampleName: '', status: 'empty', message: 'Open the plugin to load a sample.', durationSeconds: 0, sampleRate: 0, masterGain: 1 }
const nativeAvailable = () => Boolean((window as Window & { __JUCE__?: unknown }).__JUCE__)
export async function readSnapshot(): Promise<Snapshot> { return nativeAvailable() ? await getNativeFunction('getSnapshot')() as Snapshot : demo }
export async function chooseSample(): Promise<void> { if (nativeAvailable()) await getNativeFunction('chooseSample')() }
export async function audition(): Promise<void> { if (nativeAvailable()) await getNativeFunction('audition')() }
export async function setMasterGain(value: number): Promise<void> { if (nativeAvailable()) await getNativeFunction('setMasterGain')({ schemaVersion: 1, value }) }
