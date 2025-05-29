#pragma once
#include <queue>
#include <mutex>

template<typename T>
class ThreadsafeQueue
{
private:
	std::queue<T> que;
	std::mutex mtx;

public:
	bool empty() 
	{
		std::lock_guard<std::mutex> lock(mtx);
		return que.empty();
	}

	void push(T value)
	{
		std::lock_guard<std::mutex> lock(mtx);
		que.push(value);
	}

	bool try_pop(T& value)
	{
		bool result{};
		std::lock_guard<std::mutex> lock(mtx);
		if (!que.empty())
		{
			value = que.front();
			que.pop();
			result = true;
		}
		return result;
	}
};