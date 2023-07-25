<template>
  <div>
    <h1>
      <span>注册</span>
    </h1>
    <form @submit.prevent="signUp">
      <p><label for="username">学号</label> <input id="username" v-model="username" required minlength="12" maxlength="12" pattern="\d{12}"></p>
      <p><label for="nickname">昵称</label> <input id="nickname" v-model="nickname" required minlength="2" maxlength="42"></p>
      <p><label for="password">密码</label> <input id="password" v-model="password" required type="password" minlength="8"></p>
      <p>
        <button type="submit">注册</button>
        <span><NuxtLink class="button" to="/login">转到登录</NuxtLink></span>
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
const nickname = ref('')
const password = ref('')
const router = useRouter()

const signUp = async () => {
  const resp = await fetch(new URL('/user', base), {
    method: 'post',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ id: username.value, password: password.value, name: nickname.value }),
  })
  if (resp.status === 409) {
    alert('此学号已经注册')
    return
  }
  if (resp.status !== 200 && resp.status !== 201) {
    alert(`未知错误: ${resp.status} (${resp.statusText})`)
    return
  }
  alert('注册成功')
  router.push('/login')
}
</script>

