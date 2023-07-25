<template>
  <div>
    <h1 class="nav">
      <span>登录</span>
    </h1>
    <form @submit.prevent="logIn">
      <p><label for="username">学号</label> <input id="username" v-model="username" required></p>
      <p><label for="password">密码</label> <input id="password" v-model="password" type="password" required></p>
      <p>
        <button type="submit">登录</button>
        <span><NuxtLink class="button" to="/signup">转到注册</NuxtLink></span>
      </p>
    </form>
  </div>
</template>

<style scoped>
.button {
  margin-left: 16px;
}
</style>

<script setup>
import { base } from '~/lib/fetch'

const username = ref('')
const password = ref('')
const router = useRouter()

const logIn = async () => {
  const resp = await fetch(new URL('/token', base), {
    method: 'post',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ id: username.value, password: password.value }),
  })
  if (resp.status === 401) {
    alert('用户名或密码错误，请重试。')
    return
  }
  if (resp.status !== 200) {
    alert(`未知错误: ${resp.status} (${resp.statusText})`)
    return
  }
  const token = await resp.text()
  localStorage.token5b = token
  router.push('/')
}
</script>
