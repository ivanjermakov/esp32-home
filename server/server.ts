import { createReadStream } from 'fs'
import { IncomingMessage, ServerResponse, createServer } from 'http'
import { extname, join, normalize } from 'path'
import { CronJob } from 'cron'
import { stat } from 'fs/promises'
import { exit } from 'process'
import { WebSocketServer } from 'ws'
import { WebSocket } from 'ws'
import { Device, Trigger, deviceSchema } from './api'
import { db, initDb, sql } from './db'
import { debug, error, info, request } from './log'
import { assertSearchParams } from './url'

type TriggerInstance = {
    trigger: Trigger
    job: CronJob
}

const streamFile = (filePath: string, res: ServerResponse): void => {
    const ext = extname(filePath).toLowerCase()
    const ctype = contentType[ext as keyof typeof contentType] ?? contentType['.txt']
    res.setHeader('Content-Type', ctype)
    const stream = createReadStream(filePath)
    stream.on('error', () => {
        res.statusCode = 500
        res.end('Server error')
    })
    stream.pipe(res)
}

const contentType = {
    '.html': 'text/html; charset=utf-8',
    '.htm': 'text/html; charset=utf-8',
    '.css': 'text/css; charset=utf-8',
    '.js': 'application/javascript; charset=utf-8',
    '.json': 'application/json; charset=utf-8',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.jpeg': 'image/jpeg',
    '.gif': 'image/gif',
    '.svg': 'image/svg+xml',
    '.txt': 'text/plain; charset=utf-8',
    '.wav': 'audio/wav',
    '.mp4': 'video/mp4',
    '.woff': 'font/woff',
    '.woff2': 'font/woff2'
}

const readBody = (req: IncomingMessage): Promise<ArrayBuffer> => {
    return new Promise<ArrayBuffer>((resolve, reject) => {
        const chunks: Buffer[] = []
        req.on('data', chunk => chunks.push(chunk))
        req.on('end', () => resolve(joinBuffers(chunks)))
        req.on('error', reject)
    })
}

const joinBuffers = (buffers: Buffer[]): ArrayBuffer => {
    const totalLength = buffers.reduce((sum, b) => sum + b.byteLength, 0)
    const result = new Uint8Array(totalLength)
    let offset = 0
    for (const buf of buffers) {
        result.set(buf, offset)
        offset += buf.byteLength
    }
    return result.buffer
}

const tryServeFile = async (url: string | undefined, res: ServerResponse): Promise<boolean> => {
    try {
        let urlPath = decodeURIComponent(url ?? '/')
        if (urlPath === '/') urlPath = '/index.html'
        const truePath = normalize(join(distPath, urlPath))
        if (!truePath.startsWith(normalize(`${distPath}/`))) return false

        const stats = await stat(truePath)
        if (stats.isFile()) {
            streamFile(truePath, res)
            return true
        }
    } catch (e) {}
    return false
}

const handleRequest = async (req: IncomingMessage, res: ServerResponse): Promise<void> => {
    request(req)
    const host = req.headers.host ?? 'localhost'
    const rawUrl = `http://${host}${req.url ?? '/'}`
    const url = new URL(rawUrl)

    if (req.method === 'GET' && url.pathname === '/devices') {
        const devices: Device[] = await Promise.all(
            Object.entries(deviceSchema).map(async ([name, actions]) => {
                const raw = await db.all(sql`select id, body from Trigger where device = ?`, name)
                const triggers = raw.map(row => ({ ...JSON.parse(row.body), id: row.id }))
                return {
                    name: name as keyof typeof deviceSchema,
                    actions: actions as any,
                    live: clients[name].length > 0,
                    triggers
                }
            })
        )
        res.setHeader('Content-Type', contentType['.json'])
        res.write(JSON.stringify(devices))
        res.statusCode = 200
        res.end()
        return
    }
    if (req.method === 'POST' && url.pathname === '/trigger') {
        const trigger: Trigger = JSON.parse(new TextDecoder().decode(await readBody(req)))
        const insert = await db.run(
            sql`insert into Trigger (device, body) values (?, ?)`,
            trigger.device,
            JSON.stringify(trigger)
        )

        trigger.id = insert.lastID!
        triggerInstance[trigger.id] = { trigger, job: spawnJob(trigger) }

        res.setHeader('Content-Type', contentType['.json'])
        res.write(JSON.stringify(trigger))
        res.statusCode = 201
        res.end()
        return
    }
    if (req.method === 'PUT' && url.pathname === '/trigger') {
        const trigger: Trigger = JSON.parse(new TextDecoder().decode(await readBody(req)))
        await db.run(sql`update Trigger set body = ? where id = ?`, JSON.stringify(trigger), trigger.id)

        const instance = triggerInstance[trigger.id]
        instance.trigger = trigger
        instance.trigger.enabled ? instance.job.start() : instance.job.stop()

        res.setHeader('Content-Type', contentType['.json'])
        res.write(JSON.stringify(trigger))
        res.statusCode = 201
        res.end()
        return
    }
    if (req.method === 'DELETE' && url.pathname === '/trigger') {
        const params = assertSearchParams(url, ['id'])
        await db.run(sql`delete from Trigger where id = ?`, params.id)

        const instance = triggerInstance[Number.parseInt(params.id)]
        instance.job.stop()
        delete triggerInstance[Number.parseInt(params.id)]

        res.statusCode = 204
        res.end()
        return
    }
    if (req.method === 'POST' && url.pathname === '/action') {
        const params = assertSearchParams(url, ['device', 'action'])
        run(params.device, params.action)
        res.statusCode = 201
        res.end()
        return
    }
    if (req.method === 'GET' && url.pathname === '/jobs') {
        res.setHeader('Content-Type', contentType['.json'])
        res.write(
            JSON.stringify(
                Object.values(triggerInstance).map(i => ({
                    trigger: i.trigger,
                    last: i.job.lastDate(),
                    next: i.job.nextDates(10)
                }))
            )
        )
        res.statusCode = 200
        res.end()
        return
    }

    if (await tryServeFile(url.pathname, res)) {
        return
    }

    if (url.pathname === '/' && (await tryServeFile('/', res))) {
        return
    }

    res.statusCode = 404
    res.end()
}

const run = (device: string, action: string) => {
    info(`run ${device}/${action}`)
    const cs = clients[device]
    if (cs.length === 0) throw Error('no device')
    const actions = deviceSchema[device as keyof typeof deviceSchema]
    cs.forEach(c => c.send(new Uint8Array([actions.indexOf(action as any)])))
}

const onTick = (trigger: Trigger) => {
    info('tick', trigger)
    if (!trigger.enabled) return
    try {
        trigger.actions.forEach(action => run(trigger.device, action))
    } catch (e) {
        error('tick failed', e)
    }
}

const spawnJob = (trigger: Trigger) =>
    CronJob.from({ cronTime: trigger.cron, onTick: () => onTick(trigger), start: trigger.enabled })

const distPath = process.env.HOME_DIST!
if (!distPath) {
    error('no dist path')
    exit(1)
}

let deinitizlized = false
const deinit = async (): Promise<void> => {
    if (deinitizlized) return
    deinitizlized = true
    debug('deinitializing')

    await new Promise<void>((resolve, reject) =>
        server.listening ? server.close(e => (e ? reject(e) : resolve())) : resolve()
    )
    await db.close()
    info('deinitialized')
    exit(0)
}
process.on('SIGINT', deinit)
process.on('SIGTERM', deinit)

await initDb()

const server = createServer((req, res) => {
    handleRequest(req, res).catch(e => {
        error('request error', e)
        res.setHeader('Content-Type', contentType['.txt'])
        res.statusCode = 500
        res.end(e.message ?? 'Server error')
    })
})

const clients: { [name: string]: (WebSocket & { isAlive?: boolean })[] } = Object.fromEntries(
    ['', ...Object.keys(deviceSchema)].map(name => [name, []])
)
const wsServer = new WebSocketServer({ server })
wsServer.on('connection', (ws: WebSocket & { isAlive?: boolean }, req) => {
    request(req)
    const host = req.headers.host ?? 'localhost'
    const rawUrl = `http://${host}${req.url ?? '/'}`
    const url = new URL(rawUrl)
    const name = url.pathname.slice(1)

    ws.isAlive = true
    clients[name].push(ws)
    debug(`client "${name}" connected`)

    ws.on('message', (e: Buffer) => {
        debug('msg', e.toString())
        ws.send(e)
    })
    ws.on('pong', () => {
        ws.isAlive = true
    })
    ws.on('close', () => {
        debug(`client "${name}" disconnected`)
        const i = clients[name].indexOf(ws)
        if (i >= 0) clients[name].splice(i, 1)
    })
})

const port = Number.parseInt(process.env.HOME_PORT ?? '3000')
server.listen(port, () => {
    info(`server started :${port}`)
})

setInterval(() => {
    Object.entries(clients).forEach(([name, group]) =>
        group.forEach(ws => {
            if (!ws.isAlive) {
                debug(`client "${name}" heartbeat failed`)
                const i = clients[name].indexOf(ws)
                if (i >= 0) clients[name].splice(i, 1)
            }
            ws.ping()
            ws.isAlive = false
        })
    )
}, 10e3)

const triggerInstance: { [id: number]: TriggerInstance } = Object.fromEntries(
    (await db.all(sql`select * from Trigger`)).map(row => {
        const trigger: Trigger = { ...JSON.parse(row.body), id: row.id }
        const ti: TriggerInstance = {
            trigger,
            job: spawnJob(trigger)
        }
        return [row.id, ti]
    })
)
