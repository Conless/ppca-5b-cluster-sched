<template>
  <div>
    <Nav />
    <table>
      <thead>
        <th>#</th>
        <th>昵称</th>
        <th>云厂商</th>
        <th>客户</th>
        <th>总分</th>
      </thead>
      <tbody>
        <tr v-for="user, i in scoreboard" :class="{ current: user.isCurrent }" :key="i">
          <td>{{ i + 1 }}</td>
          <td>{{ user.name }}</td>
          <td>{{ user.server?.toFixed(4) ?? '/' }}</td>
          <td>{{ user.client?.toFixed(4) ?? '/' }}</td>
          <td>{{ user.total?.toFixed(4) }}</td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<style scoped>
.current {
  background-color: rgba(64, 128, 84, .1);
}
@media(prefers-color-scheme: dark) {
.current {
  background-color: rgba(64, 128, 84, .5);
}
}
</style>

<script setup>
import { request } from '~/lib/fetch'

const scoreboard = await (await request('/scoreboard')).json()
</script>
