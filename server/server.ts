import { createReadStream } from 'fs'
import { IncomingMessage, ServerResponse, createServer } from 'http'
import { extname, join, normalize } from 'path'
import { stat } from 'fs/promises'
import { exit } from 'process'
import { WebSocketServer } from 'ws'
import { WebSocket } from 'ws'
import { Device, deviceSchema } from './api'
import { debug, error, info, request } from './log'
import { assertSearchParams } from './url'

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
        const devices: Device[] = Object.entries(deviceSchema).map(([name, actions]) => ({
            name: name as keyof typeof deviceSchema,
            actions: actions as any,
            live: clients[name].length > 0
        }))
        res.setHeader('Content-Type', contentType['.json'])
        res.write(JSON.stringify(devices))
        res.statusCode = 200
        res.end()
    }

    if (req.method === 'POST' && url.pathname === '/action') {
        const params = assertSearchParams(url, ['device', 'action'])
        const cs = clients[params.device]
        if (cs.length === 0) throw Error('no device')
        const actions = deviceSchema[params.device as keyof typeof deviceSchema]
        cs.forEach(c => c.send(new Uint8Array([actions.indexOf(params.action as any)])))
        res.statusCode = 200
        res.end()
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

const distPath = process.env.HOME_DIST!
if (!distPath) {
    error('no dist path')
    exit(1)
}

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
