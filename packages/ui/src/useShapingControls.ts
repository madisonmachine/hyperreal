import { useEffect, useRef, useState } from 'react'
import type { Snapshot } from './bridge'

type Awaiting = { value: number; since: number }

export function useShapingControls<T extends Record<keyof T, number>>(
  snapshot: Snapshot,
  fromSnapshot: (snapshot: Snapshot) => T,
  send: (pad: number, field: keyof T & string, value: number) => Promise<boolean>,
  errorLabel: string
) {
  type Field = keyof T & string
  const [values, setValues] = useState<T>(() => fromSnapshot(snapshot))
  const [error, setError] = useState('')
  const latestSnapshot = useRef(snapshot)
  const pending = useRef(new Map<Field, number>())
  const awaiting = useRef(new Map<Field, Awaiting>())
  const sending = useRef(false)
  const sendingField = useRef<Field | null>(null)
  const alive = useRef(true)

  latestSnapshot.current = snapshot
  useEffect(() => {
    alive.current = true
    return () => { alive.current = false }
  }, [])
  useEffect(() => {
    const incoming = fromSnapshot(snapshot)
    setValues(current => {
      const next = { ...current }
      let changed = false
      for (const field of Object.keys(incoming) as Field[]) {
        if (pending.current.has(field) || sendingField.current === field) continue
        const wait = awaiting.current.get(field)
        if (wait) {
          const tolerance = Math.max(0.002, Math.abs(wait.value) * 0.001)
          if (Math.abs(incoming[field] - wait.value) <= tolerance)
            awaiting.current.delete(field)
          else if (Date.now() - wait.since < 250) continue
          else awaiting.current.delete(field)
        }
        if (next[field] !== incoming[field]) { next[field] = incoming[field]; changed = true }
      }
      return changed ? next : current
    })
  }, [snapshot, fromSnapshot])

  const flush = async () => {
    if (sending.current) return
    sending.current = true
    while (pending.current.size > 0) {
      // One native change per frame at most; newer values replace queued slider events.
      await new Promise<void>(resolve => window.setTimeout(resolve, 16))
      const [field, value] = pending.current.entries().next().value as [Field, number]
      pending.current.delete(field)
      sendingField.current = field
      let accepted = false
      try { accepted = await send(snapshot.selectedPad, field, value) } catch { /* reported below */ }
      sendingField.current = null
      if (accepted) awaiting.current.set(field, { value, since: Date.now() })
      else if (alive.current) {
        setError(`${errorLabel} CHANGE FAILED`)
        if (!pending.current.has(field)) {
          const actual = fromSnapshot(latestSnapshot.current)[field]
          setValues(current => ({ ...current, [field]: actual }))
        }
      }
    }
    sending.current = false
  }
  const change = (field: Field, value: number) => {
    setValues(current => ({ ...current, [field]: value }))
    setError('')
    pending.current.set(field, value)
    void flush()
  }
  return { values, error, change }
}
