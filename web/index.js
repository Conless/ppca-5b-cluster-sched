import Router from '@koa/router'
import Nedb from '@seald-io/nedb'
import * as argon2 from 'argon2'
import S3 from 'aws-sdk/clients/s3.js'
import { config } from 'dotenv'
import * as jwt from 'jsonwebtoken'
import Koa from 'koa'
import { koaBody } from 'koa-body'
import { v4 as uuid } from 'uuid'
import logger from 'koa-logger'
import fetch from 'node-fetch'
// import serve from 'koa-static'

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

app.use(logger()).use(koaBody({ json: true }))
app.use(router.routes()).use(router.allowedMethods())
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
  await db.insertAsync({ is: 'user', id, name, password: argon2.hash(password) })
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
  if (!argon2.verify(user.password, password)) ctx.throw(401)
  ctx.body = jwt.sign({ sub: id, exp: Math.floor(Date.now() / 1000) + 86400 }, secret)
})

/** @type {Koa.Middleware} */
const auth = async (ctx, next) => {
  ctx.state.user = 'xxx'
  return await next()
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
  if (typeof name !== 'string' || !name || name.length > 42) ctx.throw(400)
  await db.updateAsync({ is: 'user', id }, { $set: { name } })
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

const s3c = {
  buckets: {
    usercontent: '5buc',
    testcases: '5bt',
  },
  checker: {
    ac: new SourceLocation(s3c.bucket.testcases, 'ac'),
    checkAns: new SourceLocation(s3c.bucket.testcases, 'checkans'),
    normalize: new SourceLocation(s3c.bucket.testcases, 'normalize'),
  },
}

router.get('/scoreboard', auth, async ctx => {
  // TODO
})

router.get('/code/:type(cheat|anticheat)', auth, async ctx => {
  const { type } = ctx.params
  const { user } = ctx.state
  const code = await db.findOneAsync({ is: 'code', type, user }).execAsync()
  if (!code) ctx.throw(404)
  if (!code.id) {
    ctx.body = ''
    return
  }
  ctx.redirect(await s3.getSignedUrlPromise('getObject', {
    Bucket: s3c.buckets.usercontent,
    Key: code.id,
    Expires: 60,
  }))
})

router.get('/code/upload', auth, async ctx => {
  const id = uuid()
  ctx.body = {
    id,
    url: await s3.getSignedUrlPromise('putObject', {
      Bucket: s3c.buckets.usercontent,
      Key: `${ctx.state.user}/${id}.cpp`,
      Expires: 60,
    }),
  }
})

router.put('/code/:type(cheat|anticheat)/:id', auth, async ctx => {
  const { type, id: postfixId } = ctx.params
  const { user } = ctx.state
  const id = `${user}/${postfixId}`
  try {
    const head = await s3.headObject({ Bucket: s3c.buckets.usercontent, Key: id + '.cpp' }).promise()
    if (head.ContentLength > 4 * 1024 * 1024) {
      await s3.deleteObject({ Bucket: s3c.buckets.usercontent, Key: id + '.cpp' }).promise()
      ctx.throw(400)
    }
  } catch (e) {
    if (e.code === 'NotFound') ctx.throw(404)
  }
  await db.insertAsync({ is: 'version', time: new Date(), type, id, user, status: 'pending' })
  await db.updateAsync({ is: 'code', type, user }, { $set: { id } })
  await db.updateAsync({ is: 'queue' }, { $push: { queue: { type, user, id } } })
  ctx.status = 204
})

const callScheduler = async (path, req) =>
  await (await fetch(new URL(path, schedulerBase), { headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(req) })).json()
const compile = async id => {
  const req = new CompileRequest()
  req.artifact = new SourceLocation(s3c.buckets.usercontent, id)
  req.source = new SourceLocation(s3c.buckets.usercontent, id + '.cpp')
  const res = await callScheduler('/compile', id)
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
  // TODO: calculate scoreboard
}

const nTestcases = 4
const normalizeScore = score => Math.min(Math.max(score, 1), 0)

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
      await db.updateAsync({ is: 'version', id }, { $set: { status, message } })
    }

    await updateStatus('compiling')
    try {
      await compile(base)
    } catch (e) {
      await updateStatus('compile_error', String(e))
    }

    await updateStatus('judging')
    try {
      if (type === 'cheat') {
        await judgeCheat({ user, id })
      } else {
        await judgeAnticheat({ user, id })
      }
    } catch (e) {
      await updateStatus('error', String(e))
    }
    await updateStatus('done')
    await db.updateAsync({ is: 'queue' }, { $pop: { queue: -1 } })
  }
}

judgeWorker()
