#pragma once

#include "definition.h"
#include "interface.h"

#include <queue>
#include <map>

namespace oj {

  auto generate_tasks(const Description &desc) -> std::vector<Task> {
    std::vector<Task> ret;
    size_t average_time = std::max(desc.execution_time_single.min, desc.execution_time_sum.min / desc.task_count + 1);
    size_t average_priority = desc.priority_single.min;
    for (size_t i = 0; i < desc.task_count; i++) {
      ret.push_back(
        {0, desc.deadline_time.max, average_time, average_priority});
    }
    return ret;
  }

} // namespace oj

namespace oj {
  std::queue<std::pair<size_t, Task>> q;
  task_id_t task_id = 0;
  bool free[PublicInformation::kCPUCount];
  size_t free_cpu = PublicInformation::kCPUCount;
  std::map<size_t, std::vector<size_t>> savings;

  auto schedule_tasks(time_t time, std::vector<Task> list, const Description &desc) -> std::vector<Policy> {
    std::vector<Policy> ret;
    for (size_t i = 0; i < list.size(); i++) {
      q.emplace(task_id + i, list[i]);
    }
    for (size_t id: savings[time]) {
      ret.emplace_back(Saving{id});
    }
    free_cpu += savings[time - PublicInformation::kSaving].size();
    while (!q.empty() && free_cpu > 0) {
      auto t = q.front();
      q.pop();
      ret.emplace_back(Launch{1, t.first});
      savings[time + PublicInformation::kStartUp + t.second.execution_time].push_back(t.first);
      free_cpu--;
    }
    task_id += list.size();
    return ret;
  }

} // namespace oj
