<template>
  <div>
    <Nav />
    <form @submit.prevent="submit">
      <p>
        <label for="type">题目：</label>
        <select id="type" v-model="type">
          <option value="cheat">抄袭</option>
          <option value="anticheat">查重</option>
        </select>
        <button @click.prevent="load">加载上一次提交</button>
      </p>
      <p><textarea v-model="code"></textarea></p>
      <p>此题评测时间较长，<strong>5 分钟内同一题目 (抄袭/查重) 只可提交一次</strong>。(若代码产生编译错误，则不计入时限，可以直接重新提交。)</p>
      <p class="submit-line"><button type="submit" class="submit" :disabled="inProgress">提交</button></p>
    </form>
  </div>
</template>

<style scoped>
.submit-line {
  text-align: right;
}
.submit {
  color: #3c4cf2 !important;
  opacity: 1 !important;
  border: 1px solid #777;
  margin-right: 0;
  padding: 4px 8px;
}
p {
  text-align: justify;
}
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
  .submit {
    color: #3391ff !important;
    border: 1px solid #777;
  }
}
</style>

<script setup>
import { request } from '~/lib/fetch'

const type = ref('')
const code = ref('')
const inProgress = ref(false)
const router = useRouter()

const load = async () => {
  const resp = await request(`/code/${type.value}`)
  if (!resp) {
    code.value = ''
    return
  }
  code.value = await resp.text()
}

const submit = async () => {
  inProgress.value = true

  try {
    if (!type.value) {
      alert('请选择要提交的题目')
      return
    }

    if (code.value.length === 0) {
      alert('请输入代码')
      return
    }

    if (code.value.length > 4 * 1024 * 1024) {
      alert('代码过长')
      return
    }

    if (!confirm('确定要提交吗？5 分钟内无法再次提交同一题的代码。(编译错误的代码不计时间；不同题目仍可提交。)')) return

    const { id, url } = await (await request('/code/upload')).json()
    const uploadRes = await fetch(url, { method: 'put', body: code.value })
    if (uploadRes.status >= 400) {
      alert(`未知错误: ${uploadRes.status} (${uploadRes.statusText})`)
      return
    }
    await request(`/code/${type.value}/${id}`, { method: 'put' })
    alert('提交成功')
    router.push('/versions')
  } catch (e) {
    console.error(e)
  } finally {
    inProgress.value = false
  }
}
</script>

