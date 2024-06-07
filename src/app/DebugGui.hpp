#pragma once

#include "Camera.hpp"

#include "imgui.h"

#include <string>
#include <mutex>
#include <atomic>
#include <array>
#include <algorithm>
#include <numeric>

template <typename T>
class Atomic
{

public:

	Atomic() = default;
	Atomic(T value) : m_value(value) {}

	T get() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_value;
	}

	void set(T value)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_value = value;
	}

	operator T() const
	{
		return get();
	}

	Atomic<T> & operator=(T value)
	{
		set(value);
		return *this;
	}

private:

	T m_value;
	mutable std::mutex m_mutex;

};

template <typename T, int N>
class History
{

public:

	void push(T value)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_sum += value;
		m_sum -= m_history[0];
		std::shift_left(m_history.begin(), m_history.end(), 1);
		m_history[N - 1] = value;
	}

	T average() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_sum / N;
	}

	std::unique_lock<std::mutex> lock() const
	{
		return std::unique_lock<std::mutex>(m_mutex);
	}

	T * data()
	{
		return m_history.data();
	}

	int size() const
	{
		return N;
	}

private:

	std::array<T, N> m_history;
	T m_sum = 0;
	mutable std::mutex m_mutex;

};

class DebugGui
{

public:

	DebugGui();

	~DebugGui();

	void updateImGui();

	static inline std::atomic<uint32_t> fps = 0;
	static inline std::atomic<uint32_t> ups = 0;
	static inline std::atomic<uint64_t> rendered_triangles = 0;
	static inline std::atomic<uint64_t> gpu_allocated_memory = 0;

	static inline Atomic<glm::vec3> player_position;
	static inline Atomic<glm::vec3> player_velocity_vec;
	static inline std::atomic<double> player_velocity;

	static inline std::atomic<double> acceleration = 30.0;
	static inline std::atomic<double> ground_friction = 10.0;
	static inline std::atomic<double> air_friction = 0.8;
	static inline std::atomic<double> jump_force = 9.0;
	static inline std::atomic<double> gravity = 25.0;

	// Render Thread times
	static inline History<float, 100> frame_time_history;
	static inline History<float, 100> cpu_time_history;

	static inline std::atomic_int32_t chunk_mesh_count = 0;

	static inline History<float, 100> chunk_count_history;

	static inline History<float, 1000> chunk_load_queue_size_history;
	static inline History<float, 1000> chunk_unload_queue_size_history;

	static inline History<float, 100> chunk_render_time_history;
	static inline History<float, 100> chunk_gen_time_history;
	static inline History<float, 100> chunk_unload_time_history;

	static inline std::atomic<double> create_mesh_time;

	static inline std::atomic<double> store_mesh_time;
	static inline std::atomic<double> store_mesh_mutex_wait_time;
	static inline std::atomic<double> store_mesh_create_staging_buffer_time;
	static inline std::atomic<double> store_mesh_memcpy_time;
	static inline std::atomic<double> store_mesh_create_buffer_time;
	static inline std::atomic<double> store_mesh_copy_buffer_time;
	static inline std::atomic<double> store_mesh_destroy_buffer_time;

	static inline std::atomic<size_t> send_buffer_size;
	static inline std::atomic<size_t> recv_buffer_size;

	static inline History<size_t, 100> send_history;
	static inline History<size_t, 100> recv_history;
};
