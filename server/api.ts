export const deviceSchema = {
    ac: ['on', 'off']
} as const

export type Device = {
    name: keyof typeof deviceSchema
    actions: string[]
    live: boolean
    triggers: Trigger[]
}

export type Trigger = {
    id: number
    device: keyof typeof deviceSchema
    enabled: boolean
    actions: string[]
    cron: string
}
