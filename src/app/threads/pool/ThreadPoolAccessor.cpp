#include "ThreadPoolAccessor.hpp"

ThreadPoolAccessor::ThreadPoolAccessor()
: m_thread_pool(ThreadPool::get_instance())
{
}

ThreadPoolAccessor::~ThreadPoolAccessor()
{
	waitForAll();
}

void ThreadPoolAccessor::waitForAll()
{
	while (!m_futures.empty())
	{
		auto it = m_futures.begin();
		waitTask(it->first);
	}
}

void ThreadPoolAccessor::waitForFinishedTasks()
{
	std::lock_guard<std::mutex> lock(m_futures_mutex);
	std::lock_guard<std::mutex> lock2(m_finished_tasks_mutex);
	while (!m_finished_tasks.empty())
	{
		uint64_t id = *m_finished_tasks.begin();
		m_finished_tasks.erase(m_finished_tasks.begin());

		auto & future = m_futures.at(id);
		future.get();
		m_futures.erase(id);
	}
}

void ThreadPoolAccessor::waitForTask(uint64_t id)
{
	std::lock_guard<std::mutex> lock(m_futures_mutex);
	waitTask(id);
}

void ThreadPoolAccessor::waitForTasks(const std::vector<uint64_t> & ids)
{
	std::lock_guard<std::mutex> lock(m_futures_mutex);
	for (auto id : ids)
		waitTask(id);
}

void ThreadPoolAccessor::waitTask(uint64_t id)
{
	// LOG_INFO("Waiting for task " << id);
	auto & future = m_futures.at(id);
	future.get();
	m_futures.erase(id);
	{
		std::lock_guard<std::mutex> lock(m_finished_tasks_mutex);
		m_finished_tasks.erase(id);
	}
}
