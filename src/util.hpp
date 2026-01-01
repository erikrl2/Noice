#pragma once
#include <glm/glm.hpp>

#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

bool ReadFileBytes(const char* path, std::vector<unsigned char>& out);

std::string ReadFileString(const char* path);

bool ImGuiDirection2D(const char* label, glm::vec2& dir, float radius = 32.0f);

void EnableOpenGLDebugOutput();

template<typename T>
class Queue {
public:
  void Push(T&& item) {
    std::lock_guard<std::mutex> lock(mutex);
    if (closed) return;
    queue.push(std::move(item));
  }

  std::optional<T> TryPop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty()) return std::nullopt;

    T item = std::move(queue.front());
    queue.pop();
    return item;
  }

  void Close() {
    std::lock_guard<std::mutex> lock(mutex);
    closed = true;
  }

  operator bool() { return !closed; }

private:
  std::queue<T> queue;
  std::mutex mutex;
  bool closed = false;
};
