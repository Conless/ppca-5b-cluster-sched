<template>
  <div>
    <Nav />
    <form @submit.prevent="submit">
      <p>
        <select v-model="type">
          <option value="cheat">抄袭</option>
          <option value="anticheat">查重</option>
        </select>
        <button @click.prevent="load">加载上一次提交</button>
      </p>
      <p><textarea v-model="code"></textarea></p>
      <p><button type="submit" :disabled="inProgress">提交</button></p>
    </form>
  </div>
</template>

<style scoped>
select {
  padding: 0 12px 0 4px;
  margin-right: 12px;
}
textarea {
  font-family: monospace;
  width: 100%;
  resize: vertical;
  border: 1px solid #ccc;
  border-radius: 4px;
  max-width: 100%;
  height: 42em;
  padding: 8px;
  transition: 0.5s ease border-color;
}
textarea:hover {
  border-color: #999;
}
textarea:focus {
  border-color: #444;
}
@media (prefers-color-scheme: dark) {
  textarea {
    background-color: #121212;
    border: 1px solid #666;
  }
  textarea:hover {
    border-color: #999;
  }
  textarea:focus {
    border-color: #ccc;
  }
}
</style>

<script setup>
import { request } from '~/lib/fetch'

const type = ref('cheat')
const code = ref('')
const inProgress = ref(false)
const router = useRouter()

const load = async () => {
  const resp = await request(`/code/${type.value}`)
  if (!resp) return
  code.value = await resp.text()
}

const submit = async () => {
  inProgress.value = true

  try {
    if (code.value.length === 0) {
      alert('请输入代码')
      return
    }

    if (code.value.length > 4 * 1024 * 1024) {
      alert('代码过长')
      return
    }

    const { id, url } = await (await request('/code/upload')).json()
    const uploadRes = await fetch(url, { method: 'put', body: code.value })
    if (uploadRes.status >= 400) {
      alert(`未知错误: ${uploadRes.status} (${uploadRes.statusText})`)
      return
    }
    await request(`/code/${type.value}/${id}`, { method: 'put' })
    alert('提交成功')
    router.push('/')
  } catch (e) {
    console.error(e)
  } finally {
    inProgress.value = false
  }
}
</script>

