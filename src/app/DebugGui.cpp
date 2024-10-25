#include "DebugGui.hpp"
#include "logger.hpp"

#include <algorithm>
#include <numeric>

#define FLOAT_SLIDER(name, min, max) float name ## _f = name; ImGui::SliderFloat(#name, &name ## _f, min, max); name = name ## _f;
#define INT_SLIDER(name, min, max) int name ## _i = name; ImGui::SliderInt(#name, &name ## _i, min, max); name = name ## _i;

#define VEC3_SLIDER(name, min_x, max_x, min_y, max_y, min_z, max_z) \
	float name ## _x = name.x; \
	float name ## _y = name.y; \
	float name ## _z = name.z; \
	ImGui::SliderFloat(#name " x", &name ## _x, min_x, max_x); \
	ImGui::SliderFloat(#name " y", &name ## _y, min_y, max_y); \
	ImGui::SliderFloat(#name " z", &name ## _z, min_z, max_z); \
	name = glm::vec3(name ## _x, name ## _y, name ## _z);

DebugGui::DebugGui(
	VulkanAPI & vk
): vk(vk)
{
	// Tuto pour Augustus:

	// Créer une texture de 100x100
	imgui_texture_id = vk.createImGuiTexture(100, 100);
	// imgui_texture_id est declaré dans DebugGui.hpp, rajoute d'autres id si tu veux plus de textures

	// Clear la texture
	vk.ImGuiTextureClear(imgui_texture_id);

	// Dessiner des pixels rouges sur la texture
	for (int x = 0; x < 100; x++)
	{
		for (int y = 0; y < 100; y++)
		{
			vk.ImGuiTexturePutPixel(imgui_texture_id, x, y, 255, 0, 0);
		}
	}

	// Draw la texture dans la fenêtre ImGui (voir ligne 136)
	// vk.ImGuiTextureDraw(imgui_texture_id);

	// Normalement toutes ces fontions sont thread safe (mais j'avoue j'ai pas testé)

	// Fin du tuto pour Augustus
}

DebugGui::~DebugGui()
{
}

void DebugGui::updateImGui()
{
	// ImGui::ShowDemoWindow();

	if (ImGui::Begin("Debug"))
	{
		if (ImGui::BeginTabBar("HeadTabBar"))
		{
			if (ImGui::BeginTabItem("Game"))
			{
				ImGui::Text("Fps: %d", fps.load());
				ImGui::Text("XYZ: %.3f %.3f %.3f", player_position.get().x, player_position.get().y, player_position.get().z);
				ImGui::Text("V XYZ: %.3f %.3f %.3f", player_velocity_vec.get().x, player_velocity_vec.get().y, player_velocity_vec.get().z);
				ImGui::Text("Velocity: %.3f", player_velocity.load());
				ImGui::Text("Chunk: %f %f %f", std::floor(player_position.get().x / 16) , std::floor(player_position.get().y / 256), std::floor(player_position.get().z / 16));
				ImGui::Text("Looked face sky light: %d", looked_face_sky_light.load());
				ImGui::Text("Looked face block light: %d", looked_face_block_light.load());

				ImGui::Separator();

				ImGui::Text("Rendered triangles: %ld", rendered_triangles.load());
				ImGui::Text("Chunk meshes count: %d", chunk_mesh_count.load());

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Performance"))
			{
				if (ImGui::CollapsingHeader("Render Thread"))
				{
					ImGui::Text("Fps: %d", fps.load());

					{
						std::string average = "Average: " + std::to_string(frame_time_history.average()) + " ms";
						auto lock = frame_time_history.lock();
						ImGui::PlotHistogram("Frame time", frame_time_history.data(), frame_time_history.size(), 0, average.c_str(), 0.0f, 3000.0f, ImVec2(200, 80));
					}

					{
						std::string average = "Average: " + std::to_string(cpu_time_history.average()) + " ms";
						auto lock = cpu_time_history.lock();
						ImGui::PlotHistogram("Cpu time", cpu_time_history.data(), cpu_time_history.size(), 0, average.c_str(), 0.0f, 3000.0f, ImVec2(200, 80));
					}

					{
						std::string average = "Average: " + std::to_string(cpu_wait_time_history.average()) + " ms";
						auto lock = cpu_wait_time_history.lock();
						ImGui::PlotHistogram("Cpu wait time", cpu_wait_time_history.data(), cpu_wait_time_history.size(), 0, average.c_str(), 0.0f, 3000.0f, ImVec2(200, 80));
					}
				}
				if (ImGui::CollapsingHeader("Update Thread"))
				{
					ImGui::Text("Ups: %d", ups.load());
				}
				if (ImGui::CollapsingHeader("Block Update Thread"))
				{
					ImGui::Text("Fps: %d", fps.load());

					ImGui::Text("Chunk gen time:     %f µs", chunk_gen_time_history.average());
					ImGui::Text("Chunk unload time:  %f µs", chunk_unload_time_history.average());
					ImGui::Text("Chunk render time:  %f µs", chunk_render_time_history.average());
					ImGui::Text("  Chunk mesh create time:  %f µs", create_mesh_time.load());
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Memory"))
			{
				ImGui::Text("Gpu memory: %ld MB", gpu_allocated_memory / 1024 / 1024);

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Network"))
			{
				ImGui::Text("Send buffer size: %ld", send_buffer_size.load());
				ImGui::Text("Recv buffer size: %ld", recv_buffer_size.load());

				{
					// auto lock = send_history.lock();
					// ImGui::PlotHistogram("Send usage", send_history.data(), send_history.size());
				}

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Textures for Augustus"))
			{
				vk.ImGuiTextureDraw(imgui_texture_id);

				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Graphic"))
			{
				// slidders for atmosphere parrams
				FLOAT_SLIDER(sun_theta, 0.0f, 360.0f)
				FLOAT_SLIDER(earth_radius, 0.0f, 10000000.0f)
				FLOAT_SLIDER(atmosphere_radius, 0.0f, 10000000.0f)
				FLOAT_SLIDER(player_height, 0.0f, 10000.0f)

				glm::vec3 beta_r = beta_rayleigh.get() * 1e6f;
				VEC3_SLIDER(beta_r, 0.0f, 100.0f, 0.0f, 100.0f, 0.0f, 100.0f)
				beta_rayleigh = beta_r / 1e6f;

				float beta_m = beta_mie.get().x * 1e6f;
				ImGui::SliderFloat("Beta mie", &beta_m, 0.0f, 100.0f);
				beta_mie = glm::vec3(beta_m) / 1e6f;

				FLOAT_SLIDER(sun_intensity, 0.0f, 100.0f)
				FLOAT_SLIDER(h_rayleigh, 0.0f, 10000.0f)
				FLOAT_SLIDER(h_mie, 0.0f, 10000.0f)
				FLOAT_SLIDER(g, 0.0f, 1.0f)
				INT_SLIDER(n_samples, 0.0f, 20.0f)
				INT_SLIDER(n_light_samples, 0.0f, 20.0f)

				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

	}
	ImGui::End();
}
