<template>
  <main>
    <slot />
  </main>
</template>

<style src="~/static/base.css" />

<style scoped>
main {
  width: 100%;
}
</style>

<style>
table {
  width: 100%;
}
th, td {
  max-width: 30%;
  padding: 4px 20px 4px 4px;
}
th {
  text-align: left;
}
th {
  padding-bottom: 8px;
}
thead {
  border-bottom: 1px solid #889;
}

.nav {
  display: flex;
  justify-content: space-between;
  align-items: center;
}
</style>
