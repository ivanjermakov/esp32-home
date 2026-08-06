/* @refresh reload */

import { BsCircleFill } from 'solid-icons/bs'
import { MdRoundDelete_forever } from 'solid-icons/md'
import { Component, For, Match, Switch, createSignal, onMount } from 'solid-js'
import { render } from 'solid-js/web'
import { Device, Trigger } from './api'
import './index.css'

const Main: Component = () => {
    const [$loaded, setLoaded] = createSignal(false)
    const [$devices, setDevices] = createSignal<Device[]>([])
    const [$cron, setCron] = createSignal('0 0 * * *')
    const [$action, setAction] = createSignal('')

    onMount(async () => {
        setDevices(await (await fetch('/devices')).json())
        setLoaded(true)
    })

    const sendAction = async (device: Device, action: string) => {
        const res = await fetch(`/action?device=${device.name}&action=${action}`, { method: 'POST' })
        if (!res.ok) {
            const msg = await res.text()
            alert(`${res.status} ${msg}`)
        }
    }

    const addTrigger = async (e: Event, device: Device) => {
        e.preventDefault()
        const trigger: Trigger = {
            id: undefined!,
            device: device.name,
            enabled: false,
            actions: [$action()],
            cron: $cron()
        }
        const res = await fetch(`/trigger`, { method: 'POST', body: JSON.stringify(trigger) })
        if (!res.ok) {
            const msg = await res.text()
            alert(`${res.status} ${msg}`)
        }
        setDevices(await (await fetch('/devices')).json())
    }

    const toggleTrigger = async (trigger: Trigger, enabled: boolean) => {
        const updated = { ...trigger, enabled }
        const res = await fetch(`/trigger`, { method: 'PUT', body: JSON.stringify(updated) })
        if (!res.ok) {
            const msg = await res.text()
            alert(`${res.status} ${msg}`)
        }
        setDevices(await (await fetch('/devices')).json())
    }

    const deleteTrigger = async (trigger: Trigger) => {
        if (!confirm(`delete trigger? ${trigger.device} at ${trigger.cron}`)) return
        const res = await fetch(`/trigger?id=${trigger.id}`, { method: 'DELETE' })
        if (!res.ok) {
            const msg = await res.text()
            alert(`${res.status} ${msg}`)
        }
        setDevices(await (await fetch('/devices')).json())
    }

    return (
        <>
            <Switch>
                <Match when={$loaded()}>
                    <For each={$devices()}>
                        {device => (
                            <div class="device">
                                <main>
                                    <BsCircleFill classList={{ status: true, live: device.live }} />
                                    <span class="name">{device.name}</span>
                                    <For each={device.actions}>
                                        {action => (
                                            <button
                                                type="button"
                                                disabled={!device.live}
                                                onClick={() => sendAction(device, action)}
                                            >
                                                {action}
                                            </button>
                                        )}
                                    </For>
                                </main>
                                <div class="triggers">
                                    <For each={device.triggers}>
                                        {trigger => (
                                            <div class="trigger">
                                                <input
                                                    type="checkbox"
                                                    checked={trigger.enabled}
                                                    onClick={e =>
                                                        toggleTrigger(trigger, (e.target as HTMLInputElement).checked)
                                                    }
                                                />
                                                <span>{trigger.cron}</span>
                                                <For each={trigger.actions}>
                                                    {action => <span class="action">{action}</span>}
                                                </For>
                                                <MdRoundDelete_forever
                                                    class="delete"
                                                    onClick={() => deleteTrigger(trigger)}
                                                />
                                            </div>
                                        )}
                                    </For>
                                    <form>
                                        <input type="text" value={$cron()} onInput={e => setCron(e.target.value)} />
                                        <select value={$action()} onInput={e => setAction(e.target.value)}>
                                            <For each={device.actions}>
                                                {action => <option value={action}>{action}</option>}
                                            </For>
                                        </select>
                                        <button type="submit" onClick={e => addTrigger(e, device)}>
                                            add
                                        </button>
                                    </form>
                                </div>
                            </div>
                        )}
                    </For>
                </Match>
                <Match when={true}>loading...</Match>
            </Switch>
        </>
    )
}

render(() => <Main />, document.getElementById('root')!)
