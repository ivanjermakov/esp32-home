/* @refresh reload */

import { Component, Match, Switch, createSignal, onMount } from 'solid-js'
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
                <Match when={$loaded()}>{JSON.stringify($devices())}</Match>
                <Match when={true}>loading...</Match>
            </Switch>
        </>
    )
}

render(() => <Main />, document.getElementById('root')!)
