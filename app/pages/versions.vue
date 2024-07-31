<template>
  <div>
    <Nav />
    <table>
      <thead>
        <tr>
          <th>#</th>
          <th>类型</th>
          <th>提交时间</th>
          <th>代码</th>
          <th>状态</th>
          <th>提示信息</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="v, i in versions" :key="i">
          <td>{{ versions.length - i }}</td>
          <td>{{ v.type === 'server' ? '云厂商' : '客户' }}</td>
          <td>{{ fmt(v.time) }}</td>
          <td><NuxtLink class="button link" :to="`/version/${v.id}`">点击查看</NuxtLink></td>
          <td>
            {{ ({ done: '评测完成', error: '评测出错', compile_error: '编译错误', compiling: '编译中', judging: '评测中', pending: '等待评测' })[v.status] }}
            <button v-if="v.status !== 'done' && v.status !== 'error' && v.status !== 'compile_error'" @click="update">刷新</button>
          </td>
          <td><button v-if="v.message" @click="alert(v.message)">点击查看</button></td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<script setup>
import { request } from '~/lib/fetch';

const getVersions = async () => await (await request('/code/versions')).json()
const versions = ref(await getVersions())
const update = async () => versions.value = await getVersions()
const fmt = n => {
  const d = new Date(n)
  const pad = x => String(x).padStart(2, '0')
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}
const alert = x => window.alert(x)
</script>
