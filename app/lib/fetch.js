export const base = new URL('/OnlineJudge/', location.href)
// export const base = new URL('http://localhost:8080')

export const isLoggedIn = async () => {
  return !!localStorage.token5b
}

/**
 * @param {URL|string} path
 * @param {RequestInit} data
 */
export const request = async (path, data) => {
  if (!localStorage.token5b) {
    location = new URL('login', base)
    await new Promise(() => {})
  }
  if (path[0] === '/') path = path.slice(1)
  data = data || {}
  data.headers = data.headers || {}
  data.headers.Authorization = localStorage.token5b
  const url = new URL(path, base)
  const res = await fetch(url, data)
  if (res.status === 401) {
    localStorage.token5b = ''
    location = new URL('login', base)
    await new Promise(() => {})
  }
  if (res.status === 404) return null
  if (res.status === 429) {
    const time = new Date(Number(await res.text()))
    alert(`提交过于频繁，请在 ${time.toLocaleString()} 后再尝试提交`)
    throw new Error()
  }
  if (res.status >= 400) {
    alert(`未知错误: ${res.status} (${res.statusText})`)
    throw new Error()
  }
  return res
}
