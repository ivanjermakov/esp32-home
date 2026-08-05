export const deviceSchema = {
    ac: ['on', 'off']
} as const

export type Device = {
    name: keyof typeof deviceSchema
    actions: string[]
    live: boolean
}
