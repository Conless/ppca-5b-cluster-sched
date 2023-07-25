<template>
  <div>
    <h1 class="nav">
      <span>查看代码</span>
      <span><button @click="router.back()">返回</button></span>
    </h1>
    <pre><code>{{ code }}</code></pre>
  </div>
</template>

<script setup>
import { request } from '~/lib/fetch';

const r = useRoute()
const router = useRouter()

const getCode = async () => {
  try {
    const resp = await request(`/code/get/${r.params.id}`)
    if (!resp) {
      alert('404 Not Found')
      router.back()
    }
    return await resp.text()
  } catch (e) {
    router.back()
  }
}
const code = await getCode()
</script>
