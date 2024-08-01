#include <algorithm>
#include <optional>
#include <random>
#include <ranges>
#include <unordered_map>
#include <vector>

namespace oj {

static auto min_time_required(time_t target) -> time_t {
  std::size_t i = 0;
  while (time_policy(i, PublicInformation::kCPUCount) < target)
    ++i;
  return i * 1.1 + 1;
}

/**
 * @brief Generator side.
 * You need to generate a list of tasks based on the description.
 * Your task is to attempt to lower that SLO rate.
 * That is, you need to paralyze the scheduling of tasks.
 * @param desc Description of the tasks.
 * @return A list of tasks, which must satisfy the description.
 */
auto generate_tasks(const Description &desc) -> std::vector<Task> {
  const auto min_duration =
      1 + std::max(desc.execution_time_sum.min / desc.task_count,
                   desc.execution_time_single.min);
  const auto min_priority = 1 + desc.priority_sum.min / desc.task_count;

  const Task template_task = {.launch_time = 1,
                              .deadline = 1 + min_time_required(min_duration),
                              .execution_time = 1 + min_duration,
                              .priority = 1 + min_priority};

  std::vector<Task> tasks{desc.task_count, template_task};

  std::mt19937 gen{1919810};
  constexpr std::size_t kMagic = 8;

  for (auto &task : tasks) {
    task.deadline += gen() % kMagic;
    task.deadline = std::max(task.deadline, task.launch_time + 1);
    auto rest = desc.deadline_time.max - task.deadline;
    if (rest > 0) {
      const auto offset = gen() % rest;
      task.launch_time += offset;
      task.deadline += offset;
    }
    task.execution_time += gen() % kMagic;
    task.priority += gen() % 3;
  }

  return tasks;
}

} // namespace oj

namespace oj {

struct StateManager {
  StateManager() : cpu_usage(0) {}

  static auto my_policy(time_t duration, std::size_t cpu) -> time_t {
    constexpr auto kSaving = PublicInformation::kSaving;
    if (duration < kSaving)
      return 0;
    else
      return time_t(time_policy(duration - kSaving, cpu));
  }

  auto try_to_take(task_id_t task_id, const Task &task)
      -> std::optional<Launch> {
    const auto [launch, ddl, execute_time, _priority] = task;
    auto duration = ddl - launch;
    auto rest_cpu = PublicInformation::kCPUCount - cpu_usage;
    if (my_policy(duration, rest_cpu) < execute_time)
      return std::nullopt;

    while (my_policy(duration, rest_cpu - 1) >= execute_time) {
      rest_cpu--; // Find the minimum CPUs that can run the task.
    }

    cpu_usage += rest_cpu;

    while (my_policy(duration - 1, rest_cpu) >= execute_time) {
      duration--; // Find the minimum duration that can run the task.
    }

    const auto finish = launch + duration - PublicInformation::kSaving;

    Launch retval{.cpu_cnt = rest_cpu, .task_id = task_id};
    finish_tasks[finish].push_back(retval);
    return retval;
  }

  auto release(time_t time) -> std::vector<Policy> {
    this->release_saved(time);

    auto it = finish_tasks.find(time);
    if (it == finish_tasks.end())
      return {};

    std::vector<Policy> ret;
    ret.reserve(it->second.size());

    auto &saved = saved_tasks[time + PublicInformation::kSaving];
    for (auto [cpu_cnt, task_id] : it->second) {
      ret.push_back(Saving{.task_id = task_id});
      saved += cpu_cnt;
    }

    finish_tasks.erase(it);
    return ret;
  }

  void reset() {
    cpu_usage = 0;
    finish_tasks.clear();
    saved_tasks.clear();
  }

private:
  void release_saved(time_t time) {
    auto it = saved_tasks.find(time);
    if (it == saved_tasks.end())
      return;
    this->cpu_usage -= it->second;
    saved_tasks.erase(it);
  }

  cpu_id_t cpu_usage;
  std::unordered_map<time_t, std::vector<Launch>> finish_tasks;
  std::unordered_map<time_t, cpu_id_t> saved_tasks;
};

/**
 * @brief Scheduler side.
 * You need to arrange dynamically generated tasks.
 * Your task is to attempt to improve the SLO rate.
 * That is, you need to optimize the scheduling of tasks.
 * @param time Current time.
 * @param list A list of tasks that need to be scheduled.
 * @param desc Description of the tasks. Same as the one used in generate_tasks.
 */
auto schedule_tasks(time_t time, std::vector<Task> list)
    -> std::vector<Policy> {
  static StateManager manager;
  static std::vector<Task *> cached_list;
  static std::size_t task_count = 0;

  cached_list.resize(list.size());
  for (std::size_t i = 0; i < list.size(); i++)
    cached_list[i] = &list[i];

  std::ranges::sort(cached_list, [](const Task *lhs, const Task *rhs) {
    return lhs->launch_time < rhs->launch_time;
  });

  std::vector<Policy> ret = manager.release(time);
  for (auto *task : cached_list) {
    if (task->launch_time != time)
      throw std::runtime_error("Invalid task launch time");
    auto which = task - list.data() + task_count;
    if (auto info = manager.try_to_take(which, *task))
      ret.push_back(*info);
  }

  task_count += list.size();
  return ret;
}

} // namespace oj
