#include "ThreadPool.hpp"
#include "logger.hpp"

ThreadPool::ThreadPool()
: m_done(false), m_joiner(m_threads)
{
	unsigned const thread_count = std::thread::hardware_concurrency();
	// unsigned const thread_count = 4;
	try
	{
		for (unsigned i = 0; i < thread_count; i++)
		{
			m_threads.push_back(std::thread(&ThreadPool::worker_thread, this));
		}
	}
	catch (...)
	{
		m_done = true;
		m_cond.notify_all();
		throw;
	}
}

ThreadPool::~ThreadPool()
{
	m_done = true;
	m_cond.notify_all();
}

void ThreadPool::worker_thread()
{
	while (!m_done)
	{
		std::packaged_task<void()> task;
		{
			std::unique_lock<std::mutex> lock(m_queue_mutex);
			m_cond.wait(lock, [&] {return !m_work_queue.empty() || m_done; });
			if (m_done == true) break;
			task = std::move(m_work_queue.front());
			m_work_queue.pop();
		}
		try {
		task();
		} catch (std::exception & e) {
			LOG_CRITICAL("Error in worker thread, shutting down: " << e);
			m_done = true;
			m_cond.notify_all();
		}
	}
}
