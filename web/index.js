import cors from '@koa/cors'
import Router from '@koa/router'
import Nedb from '@seald-io/nedb'
import * as argon2 from 'argon2'
import S3 from 'aws-sdk/clients/s3.js'
import { config } from 'dotenv'
import { default as jwt } from 'jsonwebtoken'
import Koa from 'koa'
import { koaBody } from 'koa-body'
import { v4 as uuid } from 'uuid'
import logger from 'koa-logger'
import fetch from 'node-fetch'
import serve from 'koa-static'
import { start as startRepl } from 'repl'

config()

const s3 = new S3({
  endpoint: process.env.S3_ENDPOINT,
  s3ForcePathStyle: true,
  credentials: {
    accessKeyId: process.env.S3_ACCESS_KEY,
    secretAccessKey: process.env.S3_SECRET_KEY,
  },
  signatureVersion: 'v4',
})

const secret = process.env.SECRET
if (!secret) throw 'Must assign a secret'
const schedulerBase = new URL(process.env.SCHEDULER)

const app = new Koa()
const router = new Router()

const db = new Nedb({ filename: 'data.db', autoload: true })

; {
  const q = await db.findOneAsync({ is: 'queue' })
  if (!q) {
    await db.insertAsync({ is: 'queue', queue: [] })
  }
}

app.use(logger()).use(cors()).use(koaBody({ json: true }))
app.use(router.routes()).use(router.allowedMethods())
app.use(serve('../app/.output/public'))
app.listen(Number(process.env.PORT))

router.post('/user', async ctx => {
  if (!ctx.request.body) ctx.throw(400)
  const { id, name, password } = ctx.request.body
  if (typeof id !== 'string' || typeof name !== 'string' || typeof password !== 'string' || !name || !id || !password || name.length > 42) {
    ctx.throw(400)
  }
  if ((await db.findAsync({ is: 'user', id }).execAsync()).length > 0) {
    ctx.throw(409)
  }
  await db.insertAsync({ is: 'user', id, name, password: await argon2.hash(password) })
  await db.insertAsync({ is: 'code', user: id, id: null })
  ctx.status = 201
})

router.post('/token', async ctx => {
  if (!ctx.request.body) ctx.throw(400)
  const { id, password } = ctx.request.body
  if (typeof id !== 'string' || typeof password !== 'string') {
    ctx.throw(400)
  }
  const user = await db.findOneAsync({ is: 'user', id }).execAsync()
  if (!user) ctx.throw(404)
  if (!await argon2.verify(user.password, password)) ctx.throw(401)
  ctx.body = jwt.sign({ sub: id, exp: Math.floor(Date.now() / 1000) + 86400 }, secret)
})

/** @type {Koa.Middleware} */
const auth = async (ctx, next) => {
  // ctx.state.user = 'xxx'
  // return await next()
  const auth = ctx.header.authorization
  if (!auth) ctx.throw(401)
  try {
    const { sub } = jwt.verify(auth, secret)
    // const user = await db.findOneAsync({ is: 'user', id: sub }).execAsync()
    // if (!user) ctx.throw(401)
    ctx.state.user = sub
  } catch (e) {
    ctx.throw(401)
  }
  await next()
}

router.put('/name', auth, async ctx => {
  if (!ctx.request.body) ctx.throw(400)
  const { name } = ctx.request.body
  const { user } = ctx.state
  if (typeof name !== 'string' || !name || name.length > 42) ctx.throw(400)
  await db.updateAsync({ is: 'user', id: user }, { $set: { name } })
  await updateScoreboard()
  ctx.status = 204
})

class SourceLocation {
  constructor (bucket, key) {
    this.bucket = bucket
    this.key = key
  }
}

class Testpoint {
  /** @type {SourceLocation} */
  code
  /** @type {SourceLocation?} */
  checker
  /** @type {SourceLocation} */
  input
  /** @type {SourceLocation?} */
  output
  /** @type {SourceLocation?} */
  answer = null
  /** @type {SourceLocation[]} */
  supplementaryFiles = []
}

class CompileRequest {
  /** @type {SourceLocation} */
  source
  /** @type {SourceLocation} */
  artifact
}

const s3c = (() => {
  const buckets = {
    usercontent: '5buc',
    testcases: '5bt',
  }
  return {
    buckets,
    checker: {
      ac: new SourceLocation(buckets.testcases, 'ac'),
      checkAns: new SourceLocation(buckets.testcases, 'checkans'),
      normalize: new SourceLocation(buckets.testcases, 'normalize'),
    },
  }
})()

router.get('/code', auth, async ctx => {
  const { user } = ctx.state
  const code = await db.findOneAsync({ is: 'code', user }).execAsync()
  if (!code) ctx.throw(404)
  if (!code.id) {
    ctx.body = ''
    return
  }
  ctx.redirect(await s3.getSignedUrlPromise('getObject', {
    Bucket: s3c.buckets.usercontent,
    Key: `${user}/${code.id}/src.hpp`,
    Expires: 60,
  }))
})

router.get('/code/get/:id', auth, async ctx => {
  const { id } = ctx.params
  const { user } = ctx.state
  const code = await db.findOneAsync({ is: 'version', id, user }).execAsync()
  if (!code) ctx.throw(404)
  ctx.redirect(await s3.getSignedUrlPromise('getObject', {
    Bucket: s3c.buckets.usercontent,
    Key: `${user}/${id}/src.hpp`,
    Expires: 60,
  }))
})

router.get('/code/versions', auth, async ctx => {
  const { user } = ctx.state
  const versions = await db.findAsync({ is: 'version', user })
  versions.sort((a, b) => b.time - a.time)
  versions.forEach(x => {
    delete x._id
    delete x.is
    delete x.user
  })
  ctx.body = versions
})

router.get('/code/upload', auth, async ctx => {
  ctx.throw(400)
  const id = uuid()
  ctx.body = {
    id,
    url: await s3.getSignedUrlPromise('putObject', {
      Bucket: s3c.buckets.usercontent,
      Key: `${ctx.state.user}/${id}/src.hpp`,
      Expires: 60,
    }),
  }
  const a = uuid()
})

let minInterval = 5 * 60 * 1000

router.put('/code/:id', auth, async ctx => {
  // ctx.throw(400)
  const { id } = ctx.params
  const { user } = ctx.state
  const current = await db.findOneAsync({ is: 'code', user })
  if (!current) ctx.throw(401)
  if (user.startsWith('523') && current.time && Date.now() - current.time <= minInterval) {
    ctx.status = 429
    ctx.body = Number(current.time) + minInterval
    return
  }
  const exists = await db.findOneAsync({ is: 'version', id })
  if (exists) ctx.throw(409)
  const base = `${user}/${id}`
  try {
    const head = await s3.headObject({ Bucket: s3c.buckets.usercontent, Key: base + '/src.hpp' }).promise()
    if (head.ContentLength > 4 * 1024 * 1024) {
      await s3.deleteObject({ Bucket: s3c.buckets.usercontent, Key: base + '/src.hpp' }).promise()
      ctx.throw(400)
    }
  } catch (e) {
    if (e.code === 'NotFound') ctx.throw(404)
  }
  await db.insertAsync({ is: 'version', time: new Date(), id, user, status: 'pending' })
  await db.updateAsync({ is: 'code', user }, { $set: { id, time: new Date() } })
  await db.updateAsync({ is: 'queue' }, { $push: { queue: { user, id } } })
  ctx.status = 204
})

const callScheduler = async (path, req) =>
  await (await fetch(new URL(path, schedulerBase), { method: 'post', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(req) })).json()

const compileServer = async id => {
  const req = new CompileRequest()
  req.artifact = new SourceLocation(s3c.buckets.usercontent, id + '/server')
  req.source = new SourceLocation(s3c.buckets.testcases, 'server.cpp')
  req.supplementaryFiles = new SourceLocation(s3c.buckets.usercontent, id + '/src.hpp')
  const res = await callScheduler('/compile', req)
  if (res.result !== 'compiled') throw new Error(res.message)
}

const compileClient = async id => {
  const req = new CompileRequest()
  req.artifact = new SourceLocation(s3c.buckets.usercontent, id + '/client')
  req.source = new SourceLocation(s3c.buckets.testcases, 'client.cpp')
  req.supplementaryFiles = new SourceLocation(s3c.buckets.usercontent, id + '/src.hpp')
  const res = await callScheduler('/compile', req)
  if (res.result !== 'compiled') throw new Error(res.message)
}

const state = await (async () => {
  const s = await db.findOneAsync({ is: 'state' })
  if (s) return s.state
  const state = {
    server: {},
    client: {},
  }
  await db.insertAsync({ is: 'state', state })
  return state
})()
const updateState = async () => {
  await db.updateAsync({ is: 'state' }, { $set: { state } })
  updateScoreboard()
}

const nTestcases = 4
const scoreboard = {
  server: [],
  client: [],
}
const updateScoreboard = async () => {
  // server
  scoreboard.server = Object.entries(state.server)
    .map(([ user, { id, result } ]) => {
      const scores = Object.values(result)
        .flatMap(x => x.testpoints)
        .map(x => (x.score - 0.5) * 2)
      const score = scores.reduce((x, y) => x + y, 0) / nTestcases
      if (Number.isNaN(score)) return { user, id, score: 0 }
      return { user, id, score }
    }).filter(Boolean)
    .sort((a, b) => b.score - a.score)
  scoreboard.client = Object.entries(state.client)
    .map(([ user, { id, result } ]) => {
      const scores = Object.values(result)
        .flatMap(x => x.testpoints)
        .map(x => (0.5 - x.score) * 2)
      const score = scores.reduce((x, y) => x + y, 0) / nTestcases
      if (Number.isNaN(score)) return { user, id, score: 0 }
      return { user, id, score }
    }).filter(Boolean)
    .sort((a, b) => b.score - a.score)
  await Promise.all([ ...scoreboard.server, ...scoreboard.client ].map(async x => {
    const user = await db.findOneAsync({ is: 'user', id: x.user })
    if (!user) {
      x.name = 'Unknown'
    } else {
      x.name = user.name
    }
  }))
}
await updateScoreboard()

router.get('/state', auth, async ctx => {
  if (ctx.state.user.startsWith('523')) return
  ctx.body = state
})

router.get('/scoreboard1', auth, async ctx => {
  if (ctx.state.user.startsWith('523')) return
  ctx.body = scoreboard
})

router.get('/scoreboard', auth, async ctx => {
  const users = {}
  for (const k in scoreboard) {
    for (const { user, name, score } of scoreboard[k]) {
      if (!users[user]) users[user] = { user, name }
      users[user][k] = score
    }
  }
  const rank = Object.values(users)
    .map(({ user, name, server, client }) => ({ name, isCurrent: user === ctx.state.user, server, client, total: server + client }))
    .sort((a, b) => b.total - a.total)
  ctx.body = rank
})

const normalizeScore = score => Math.max(Math.min(score, 1), 0)

const judgeClient = async ({ user, id }) => {
  const base = `${user}/${id}`

  // stage 1
  const testpoints1 = Array.from(Array(nTestcases).keys())
    .flatMap(x => {
      const tp = new Testpoint()
      tp.checker = new SourceLocation(s3c.buckets.testcases, `spj_client`)
      tp.code = new SourceLocation(s3c.buckets.usercontent, `${base}/client`)
      tp.input = new SourceLocation(s3c.buckets.testcases, `${x}.in`)
      tp.output = new SourceLocation(s3c.buckets.usercontent, `${base}/${x}.out`)
      
      return [[ tp, `${x}` ]]
    })
  const res1 = await callScheduler('/run', testpoints1.map(([ tp, _ ]) => tp))
  if (res1.result) throw new Error(res1.message)

  const okId1 = new Set(res1.map((x, i) => [ x, testpoints1[i][1] ]).filter(([ x, _ ]) => x.result === 'accepted').map(([ _, i ]) => i))
  const okTestcases1 = Array.from(Array(nTestcases).keys())
    .filter(x => okId1.has(`${x}`))

  if (okTestcases1.length !== nTestcases) {
    let message = ''
    for (const i of testpoints1.keys()) {
      const [ _, tpid ] = testpoints1[i]
      const res = res1[i]
      if (res.result === 'accepted') continue
      message += `In testpoint ${tpid}, your program got verdict ${res.result}`
      if (res.message) {
        message += ` (message: ${res.message})`
      }
      message += '\n'
    }
    await db.updateAsync({ is: 'version', id }, { $set: { message } })
  }

  const testpoints2 = okTestcases1
    .flatMap(x => {
      const tp = new Testpoint()
      tp.checker = new SourceLocation(s3c.buckets.testcases, `spj_server`)
      tp.input = new SourceLocation(s3c.buckets.usercontent, `${base}/${x}.out`)
      tp.output = null
      return [[ { testcase: x }, tp ]]
    })
  const tasks = Object.entries(state.client)
    .filter(([ user_, _ ]) => user_ !== user)
    .map(([ user, { id } ]) => {
      const tps = testpoints2.map(([ l, x ]) => {
        const tp = new Testpoint()
        tp.input = x.input
        tp.checker = x.checker
        tp.code = new SourceLocation(s3c.buckets.usercontent, `${user}/${id}/server`)
        tp.output = x.output
        return [ { ...l }, tp ]
      })
      return [ { user, id, testpoints: tps.map(([ l, _ ]) => l) }, callScheduler('/run', tps.map(([ _, tp ]) => tp)) ]
    })
  const res3 = await Promise.all(tasks.map(([ _, task ]) => task))
  if (res3.some(x => x.result)) {
    throw new Error(res3.find(x => x.result).message)
  }
  const result = Object.fromEntries(res3.map((res, i) => {
    const [ meta, ] = tasks[i]
    for (const [ i, l ] of meta.testpoints.entries()) {
      l.score = normalizeScore(res[i].score)
    }
    const required = Array.from(Array(nTestcases).keys())
    required.forEach(x => {
      if (!okId1.has(`${x}`)) {
        meta.testpoints.push({ testcase: x, score: 1 })
      }
    })
    state.server[meta.user].result[user] = { user, id, testpoints: meta.testpoints }
    return [ meta.user, meta ]
  }))

  state.client[user] = { id, ok: okTestcases1, result }
  await updateState()
}

const judgeServer = async ({ user, id }) => {
  const base = `${user}/${id}`

  const tasks = Object.entries(state.client)
    .filter(([ user_, _ ]) => user_ !== user)
    .map(([ user, { id, ok } ]) => {
      const testpoints = ok.flatMap(x => {
        const tp = new Testpoint()
        tp.checker = null
        tp.code = new SourceLocation(s3c.buckets.usercontent, `${base}/server`)
        tp.input = new SourceLocation(s3c.buckets.usercontent, `${user}/${id}/${x}.out`)
        tp.output = null
        return [[ { testcase: x }, tp ]]
      })
      return [ { user, id, testpoints: testpoints.map(([ l, _ ]) => l) }, callScheduler('/run', testpoints.map(([ _, tp ]) => tp)) ]
    })
  const res = await Promise.all(tasks.map(([ _, task ]) => task))
  if (res.some(x => x.result)) {
    throw new Error(res.find(x => x.result).message)
  }
  const result = Object.fromEntries(res.map((res, i) => {
    const [ meta, ] = tasks[i]
    for (const [ i, l ] of meta.testpoints.entries()) {
      l.score = normalizeScore(res[i].score)
    }
    const required = Array.from(Array(nTestcases).keys())
    required.forEach(x => {
      if (!state.client[meta.user].ok.includes(x)) {
        meta.testpoints.push({ testcase: x, score: 1 })
      }
    })
    state.client[meta.user].result[user] = { user, id, testpoints: meta.testpoints }
    return [ meta.user, meta ]
  }))

  state.server[user] = { id, result }
  await updateState()
}

const judgeWorker = async () => {
  while (true) {
    const res = await db.findOneAsync({ is: 'queue' })
    if (!res.queue.length) {
      await new Promise(r => setTimeout(r, 100))
      continue
    }

    const { user, id } = res.queue[0]
    const base = `${user}/${id}`
    const updateStatus = async (status, message = '') => {
      await db.updateAsync({ is: 'version', id }, { $set: { status, ...(message ? { message } : {}) } })
    }

    await updateStatus('compiling')
    try {
      await compileServer(base)
      await compileClient(base)
    } catch (e) {
      await updateStatus('compile_error', String(e))
      await db.updateAsync({ is: 'code', user, id }, { $set: { time: null } })
      await db.updateAsync({ is: 'queue' }, { $pop: { queue: -1 } })
      continue
    }

    await updateStatus('judging')
    try {
      await judgeClient({ user, id })
      await judgeServer({ user, id })
      await updateStatus('done')
    } catch (e) {
      await updateStatus('error', String(e))
    }
    await db.updateAsync({ is: 'queue' }, { $pop: { queue: -1 } })
  }
}

judgeWorker()

const repl = startRepl()
repl.context.db = db
repl.context.s3 = s3
repl.context.xeval = x => {
  eval(x)
}
