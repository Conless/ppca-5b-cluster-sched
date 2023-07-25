// export const base = new URL('/', location.href)
export const base = new URL('http://localhost:8080')

export const isLoggedIn = async () => {
  return !!localStorage.token5b
}

/**
 * @param {URL|string} path
 * @param {RequestInit} data
 */
export const request = async (path, data) => {
  if (!localStorage.token5b) location = '/login'
  data = data || {}
  data.headers = data.headers || {}
  data.headers.Authorization = localStorage.token5b
  const url = new URL(path, base)
  const res = await fetch(url, data)
  if (res.status === 401) {
    localStorage.token5b = ''
    location = '/login'
  }
  if (res.status === 404) return null
  if (res.status >= 400) {
    alert(`未知错误: ${res.status} (${res.statusText})`)
  }
  return res
}
