#pragma once
#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <mutex>
#include <optional>
#include <queue>

bool ReadFileBytes(const char* path, std::vector<unsigned char>& out);

std::string ReadFileString(const char* path);

bool ImGuiDirection2D(const char* label, glm::vec2& dir, float radius = 32.0f);

void EnableOpenGLDebugOutput();

template<typename T>
class ThreadQueue {
public:
    void push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push(std::move(item));
    }

    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex);
        if (queue.empty()) return std::nullopt;

        T item = std::move(queue.front());
        queue.pop();
        return item;
    }

private:
    std::queue<T> queue;
    std::mutex mutex;
};
