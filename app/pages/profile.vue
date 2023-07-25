<template>
  <div>
    <Nav />
    <form @submit.prevent="apply">
      <p>
        <label for="nickname">修改昵称为：</label>
        <input v-model="nickname" id="nickname" minlength="2" maxlength="42">
        <button type="submit">修改</button>
      </p>
    </form>
  </div>
</template>

<style scoped>
button {
  margin-left: 4px;
}
</style>

<script setup>
import { request } from '~/lib/fetch'

const nickname = ref('')
const router = useRouter()

const apply = async () => {
  try {
    await request('/name', { method: 'put', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: nickname.value }) })
    alert('昵称已修改')
    router.push('/')
  } catch (e) {}
}
</script>
