<template>
  <h1 class="nav">
    <span>{{ routes[route] }}</span>
    <span class="body1 r">
      <span v-for="route, i in keys" :key="i">
        <NuxtLink class="button" :to="route">{{ routes[route] }}</NuxtLink>
        <span v-if="i !== keys.length - 1" class="spacer">·</span>
      </span>
    </span>
  </h1>
</template>

<style scoped>
.spacer {
  padding: 0 8px;
  user-select: none;
}
.r {
  text-align: right;
  flex: 1;
}
</style>

<script setup>
const route = useRoute().matched[0].path
const routes = {
  '/': '排名',
  '/submit': '提交代码',
  '/versions': '提交记录',
}
const keys = Object.keys(routes).filter(x => x != route)
</script>
