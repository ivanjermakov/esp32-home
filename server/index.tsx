/* @refresh reload */

import { BsCircleFill } from 'solid-icons/bs'
import { Component, For, Match, Switch, createSignal, onMount } from 'solid-js'
import { render } from 'solid-js/web'
import { Device } from './api'
import './index.css'

const Main: Component = () => {
    const [$loaded, setLoaded] = createSignal(false)
    const [$devices, setDevices] = createSignal<Device[]>([])

    onMount(async () => {
        setDevices(await (await fetch('/devices')).json())
        setLoaded(true)
    })

    return (
        <>
            <Switch>
                <Match when={$loaded()}>
                    <For each={$devices()}>
                        {device => (
                            <div class="device">
                                <BsCircleFill classList={{ status: true, live: device.live }} />
                                <span class="name">{device.name}</span>
                                <For each={device.actions}>
                                    {action => (
                                        <button type="button" disabled={!device.live}>
                                            {action}
                                        </button>
                                    )}
                                </For>
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
