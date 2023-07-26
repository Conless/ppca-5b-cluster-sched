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

const wrapUrl = url => url.replace(process.env.S3_ENDPOINT, process.env.S3_PUBLIC)

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
app.use(serve('../app/dist'))
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
  for (const type of [ 'cheat', 'anticheat' ]) {
    await db.insertAsync({ is: 'code', user: id, type, id: null })
  }
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

router.get('/code/:type(cheat|anticheat)', auth, async ctx => {
  const { type } = ctx.params
  const { user } = ctx.state
  const code = await db.findOneAsync({ is: 'code', type, user }).execAsync()
  if (!code) ctx.throw(404)
  if (!code.id) {
    ctx.body = ''
    return
  }
  ctx.redirect(wrapUrl(await s3.getSignedUrlPromise('getObject', {
    Bucket: s3c.buckets.usercontent,
    Key: `${user}/${code.id}.cpp`,
    Expires: 60,
  })))
})

router.get('/code/get/:id', auth, async ctx => {
  const { id } = ctx.params
  const { user } = ctx.state
  const code = await db.findOneAsync({ is: 'code', id, user }).execAsync()
  if (!code) ctx.throw(404)
  ctx.redirect(wrapUrl(await s3.getSignedUrlPromise('getObject', {
    Bucket: s3c.buckets.usercontent,
    Key: `${user}/${id}.cpp`,
    Expires: 60,
  })))
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
  const id = uuid()
  ctx.body = {
    id,
    url: wrapUrl(await s3.getSignedUrlPromise('putObject', {
      Bucket: s3c.buckets.usercontent,
      Key: `${ctx.state.user}/${id}.cpp`,
      Expires: 60,
    })),
  }
})

const minInterval = 5 * 60 * 1000

router.put('/code/:type(cheat|anticheat)/:id', auth, async ctx => {
  const { type, id } = ctx.params
  const { user } = ctx.state
  const current = await db.findOneAsync({ is: 'code', type, user })
  if (!current) ctx.throw(401)
  if (current.time && Date.now() - current.time <= minInterval) {
    ctx.status = 429
    ctx.body = Number(current.time) + minInterval
    return
  }
  const exists = await db.findOneAsync({ is: 'version', id })
  if (exists) ctx.throw(409)
  const base = `${user}/${id}`
  try {
    const head = await s3.headObject({ Bucket: s3c.buckets.usercontent, Key: base + '.cpp' }).promise()
    if (head.ContentLength > 4 * 1024 * 1024) {
      await s3.deleteObject({ Bucket: s3c.buckets.usercontent, Key: base + '.cpp' }).promise()
      ctx.throw(400)
    }
  } catch (e) {
    if (e.code === 'NotFound') ctx.throw(404)
  }
  await db.insertAsync({ is: 'version', time: new Date(), type, id, user, status: 'pending' })
  await db.updateAsync({ is: 'code', type, user }, { $set: { id, time: new Date() } })
  await db.updateAsync({ is: 'queue' }, { $push: { queue: { type, user, id } } })
  ctx.status = 204
})

const callScheduler = async (path, req) =>
  await (await fetch(new URL(path, schedulerBase), { method: 'post', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(req) })).json()
const compile = async id => {
  const req = new CompileRequest()
  req.artifact = new SourceLocation(s3c.buckets.usercontent, id)
  req.source = new SourceLocation(s3c.buckets.usercontent, id + '.cpp')
  const res = await callScheduler('/compile', req)
  if (res.result !== 'compiled') throw new Error(res.message)
}

const state = await (async () => {
  const s = await db.findOneAsync({ is: 'state' })
  if (s) return s.state
  const state = {
    cheat: {},
    anticheat: {},
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
  cheat: [],
  anticheat: [],
}
const updateScoreboard = async () => {
  // cheat
  scoreboard.cheat = Object.entries(state.cheat)
    .map(([ user, { id, result } ]) => {
      const scores = Object.values(result)
        .flatMap(x => x.testpoints)
        .filter(x => x.code[0] === x.code[1])
        .map(x => 1 - x.score)
      const count = Object.keys(result).length * nTestcases * 2
      const score = scores.reduce((x, y) => x + y, 0) / count
      if (Number.isNaN(score)) return { user, id, score: 0 }
      return { user, id, score }
    }).filter(Boolean)
    .sort((a, b) => b.score - a.score)
  scoreboard.anticheat = Object.entries(state.anticheat)
    .map(([ user, { id, result } ]) => {
      const scores = Object.values(result)
        .flatMap(x => x.testpoints)
        .map(x => {
          const expect = x.code[0] === x.code[1] ? 1 : -1
          const score = expect * (x.score * 2 - 1)
          return score
        })
      const score = scores.reduce((x, y) => x + y, 0) / scores.length
      if (Number.isNaN(score)) return { user, id, score: 0 }
      return { user, id, score }
    }).filter(Boolean)
    .sort((a, b) => b.score - a.score)
  await Promise.all([ ...scoreboard.cheat, ...scoreboard.anticheat ].map(async x => {
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
  if (!ctx.state.user.startsWith('521')) return
  ctx.body = state
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
    .map(({ user, name, cheat, anticheat }) => ({ name, isCurrent: user === ctx.state.user, cheat, anticheat, total: Math.max(cheat || 0, 0) + Math.max(anticheat || 0, 0) }))
    .sort((a, b) => b.total - a.total)
  ctx.body = rank
})

const normalizeScore = score => Math.max(Math.min(score, 1), 0)

const judgeCheat = async ({ user, id }) => {
  const base = `${user}/${id}`

  // stage 1
  const testpoints1 = Array.from(Array(nTestcases).keys())
    .map(x => x + 1)
    .flatMap(x => [ 'a', 'b' ].map(t => {
      const tp = new Testpoint()
      tp.checker = s3c.checker.checkAns
      tp.code = new SourceLocation(s3c.buckets.usercontent, base)
      tp.input = new SourceLocation(s3c.buckets.testcases, `${x}${t}/input.p`)
      tp.output = new SourceLocation(s3c.buckets.usercontent, `${base}-${x}${t}/output.p`)
      tp.answer = new SourceLocation(s3c.buckets.testcases, `${x}.ans`)
      return [ tp, `${x}${t}` ]
    }))
  const res1 = await callScheduler('/run', testpoints1.map(([ tp, _ ]) => tp))
  if (res1.result) throw new Error(res1.message)

  const okId1 = new Set(res1.map((x, i) => [ x, testpoints1[i][1] ]).filter(([ x, _ ]) => x.result === 'accepted').map(([ _, i ]) => i))
  const okTestcases1 = Array.from(Array(nTestcases).keys()).map(x => x + 1)
    .filter(x => [ 'a', 'b' ].every(t => okId1.has(`${x}${t}`)))

  if (okTestcases1.length !== nTestcases) {
    let message = ''
    for (const i of testpoints1.keys()) {
      const [ _, tpid ] = testpoints1[i]
      const res = res1[i]
      if (tp.result === 'accepted') continue
      message += `In testpoint ${tpid}, your program got verdict ${res.status}`
      if (res.message) {
        message += ` (message: ${res.message})`
      }
      message += '\n'
    }
    await db.updateAsync({ is: 'version', id }, { $set: { message } })
  }

  // stage 2: normalize
  const cross = [ 'aa', 'bb', 'ab', 'ba' ]
  const testpoints2 = okTestcases1
    .flatMap(x => cross.map(t => {
      const [ t1, t2 ] = t
      const tp = new Testpoint()
      tp.checker = s3c.checker.ac
      tp.code = s3c.checker.normalize
      tp.input = new SourceLocation(s3c.buckets.testcases, `${x}.ans`)
      tp.supplementaryFiles.push(new SourceLocation(s3c.buckets.testcases, `${x}${t1}/input.p`))
      tp.supplementaryFiles.push(new SourceLocation(s3c.buckets.usercontent, `${base}-${x}${t2}/output.p`))
      tp.output = new SourceLocation(s3c.buckets.usercontent, `${base}-${x}${t}/normalized.p`)
      return [ tp, `${x}${t}` ]
    }))
  const res2 = await callScheduler('/run', testpoints2.map(([ tp, _ ]) => tp))
  if (res2.result) throw new Error(res2.message)

  const okId2 = new Set(res2.map((x, i) => [ x, testpoints2[i][1] ]).filter(([ x, _ ]) => x.result === 'accepted').map(([ _, i ]) => i))
  const okTestcases2 = Array.from(Array(nTestcases).keys()).map(x => x + 1)
    .filter(x => cross.every(t => okId2.has(`${x}${t}`)))

  const testpoints3 = okTestcases2
    .flatMap(x => cross.map(t => {
      const tp = new Testpoint()
      tp.checker = null
      // tp.code = s3c.checker.normalize
      tp.input = new SourceLocation(s3c.buckets.usercontent, `${base}-${x}${t}/normalized.p`)
      tp.output = null
      return [ { testcase: x, code: t }, tp ]
    }))
  const tasks = Object.entries(state.anticheat)
    .filter(([ user_, _ ]) => user_ !== user)
    .map(([ user, { id } ]) => {
      const tps = testpoints3.map(([ l, x ]) => {
        x.code = new SourceLocation(s3c.buckets.usercontent, `${user}/${id}`)
        return [ l, x ]
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
    state.anticheat[meta.user].result[user] = { user, id, testpoints: meta.testpoints }
    return [ meta.user, meta ]
  }))

  state.cheat[user] = { id, ok: okTestcases2, result }
  await updateState()
}

const judgeAnticheat = async ({ user, id }) => {
  const base = `${user}/${id}`

  const tasks = Object.entries(state.cheat)
    .filter(([ user_, _ ]) => user_ !== user)
    .map(([ user, { id, ok } ]) => {
      const cross = [ 'aa', 'bb', 'ab', 'ba' ]
      const testpoints = ok.flatMap(x => cross.map(t => {
        const tp = new Testpoint()
        tp.checker = null
        tp.code = new SourceLocation(s3c.buckets.usercontent, base)
        tp.input = new SourceLocation(s3c.buckets.usercontent, `${user}/${id}-${x}${t}/normalized.p`)
        tp.output = null
        return [ { testcase: x, code: t }, tp ]
      }))
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
    state.cheat[meta.user].result[user] = { user, id, testpoints: meta.testpoints }
    return [ meta.user, meta ]
  }))

  state.anticheat[user] = { id, result }
  await updateState()
}

const judgeWorker = async () => {
  while (true) {
    const res = await db.findOneAsync({ is: 'queue' })
    if (!res.queue.length) {
      await new Promise(r => setTimeout(r, 100))
      continue
    }

    const { type, user, id } = res.queue[0]
    const base = `${user}/${id}`
    const updateStatus = async (status, message = '') => {
      await db.updateAsync({ is: 'version', id }, { $set: { status, ...(message ? { message } : {}) } })
    }

    await updateStatus('compiling')
    try {
      await compile(base)
    } catch (e) {
      await updateStatus('compile_error', String(e))
      await db.updateAsync({ is: 'code', type, user, id }, { $set: { time: null } })
      await db.updateAsync({ is: 'queue' }, { $pop: { queue: -1 } })
      continue
    }

    await updateStatus('judging')
    try {
      if (type === 'cheat') {
        await judgeCheat({ user, id })
      } else {
        await judgeAnticheat({ user, id })
      }
      await updateStatus('done')
    } catch (e) {
      await updateStatus('error', String(e))
    }
    await db.updateAsync({ is: 'queue' }, { $pop: { queue: -1 } })
  }
}

judgeWorker()
