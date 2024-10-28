cmake_minimum_required(VERSION 3.15)

project(ft_vox
	VERSION 0.0.1
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TRACY_ENABLE "" ON)
add_subdirectory(external/tracy)
add_subdirectory(external/freetype/freetype-2.13.3)
add_subdirectory(external/portaudio EXCLUDE_FROM_ALL)


add_executable(${PROJECT_NAME})

target_compile_options(${PROJECT_NAME}
	PRIVATE
		-O3
		-g3
		# -fsanitize=address
		# -fsanitize=thread
		-Wall
		-Wextra
		-Werror
		-Wpedantic
		-Wno-missing-braces

		# for tracy
		-Wno-missing-field-initializers
		-Wno-unused-parameter
		-Wno-empty-body
		-Wno-unused-result
		-Wno-c11-extensions
		-Wno-maybe-uninitialized
		-Wno-unused-variable
		-Wno-unused-function

		# -DNDEBUG
)

target_link_options(${PROJECT_NAME}
	PRIVATE
		# -fsanitize=thread
		# -fsanitize=address
)

target_sources(${PROJECT_NAME}
	PRIVATE
		src/main.cpp
		src/tracy_globals.cpp

		src/utils/logger.cpp

		src/app/perlin/Perlin.cpp

		src/app/application.cpp
		src/app/DebugGui.cpp
		src/app/Settings.cpp
		src/app/scenes/Camera.cpp
		src/app/scenes/WorldScene.cpp
		src/app/threads/Status.cpp
		src/app/threads/render/RenderThread.cpp
		src/app/threads/render/CascadedShadowMapping.cpp
		src/app/threads/update/UpdateThread.cpp
		src/app/threads/pool/ThreadPool.cpp
		src/app/threads/pool/ThreadPoolAccessor.cpp
		src/app/threads/block_update/BlockUpdateThread.cpp
		src/app/threads/network/NetworkThread.cpp
		src/app/vulkan/VulkanAPI.cpp
		src/app/vulkan/VulkanMemoryAllocator.cpp
		src/app/vulkan/chunk_mesh.cpp
		src/app/vulkan/vk_helper.cpp
		src/app/vulkan/Image.cpp
		src/app/vulkan/Command.cpp
		src/app/vulkan/Descriptor.cpp
		src/app/vulkan/Swapchain.cpp
		src/app/vulkan/TextRenderer.cpp
		src/app/vulkan/Buffer.cpp
		src/app/vulkan/item_mesh.cpp
		src/app/window/window.cpp
		src/app/window/input.cpp
		src/app/world/Chunk.cpp
		src/app/world/entity/Player.cpp
		src/app/world/entity/HitBox.cpp
		src/app/world/entity/Mob.cpp
		src/app/world/ClientWorld.cpp
		src/app/world/World.cpp
		src/app/world/WorldGenerator.cpp
		src/app/world/Block.cpp
		src/app/world/structures/Structures.cpp
		src/app/world/Item.cpp
		src/app/world/CreateMeshData.cpp
		src/app/network/client/Client.cpp
		src/app/network/client/ClientSocket.cpp
		src/app/network/client/ClientPacketHandler.cpp
		src/app/network/server/Server.cpp
		src/app/network/server/ServerSocket.cpp
		src/app/network/shared/Connection.cpp
		src/app/network/shared/Poller.cpp
		src/app/network/shared/Socket.cpp
		src/app/network/shared/ConnectionSocket.cpp
		src/app/network/shared/ConnectionSocket.cpp
		src/app/network/shared/PacketFactory.cpp
		src/app/network/shared/IncomingPacketList.cpp
		src/app/network/shared/packets/IPacket.cpp
		src/app/network/shared/packets/PingPacket.cpp
		src/app/network/shared/packets/DisconnectPacket.cpp
		src/app/network/shared/packets/BlockActionPacket.cpp
		src/app/network/shared/packets/PlayerConnectedPacket.cpp
		src/app/network/shared/packets/PlayerListPacket.cpp
		src/app/network/shared/packets/ChunkPacket.cpp
		src/app/network/shared/packets/ChunkRequestPacket.cpp
		src/app/network/shared/packets/ChunkUnloadPacket.cpp
		src/app/network/shared/packets/PlayerMovePacket.cpp
		src/app/network/shared/packets/ConnectionPacket.cpp
		src/app/sound/SoundEngine.cpp
		src/app/sound/AudioData.cpp
		src/app/event/AbstractEvent.cpp
		src/app/event/EventManager.cpp

		external/imgui/imgui.cpp
		external/imgui/imgui_draw.cpp
		external/imgui/imgui_widgets.cpp
		external/imgui/imgui_tables.cpp
		external/imgui/imgui_demo.cpp
		external/imgui/backends/imgui_impl_glfw.cpp
		external/imgui/backends/imgui_impl_vulkan.cpp

		external/tracy/public/TracyClient.cpp
)

# include directories (equivalent to -I)
target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/src/utils
		${CMAKE_CURRENT_SOURCE_DIR}/src/app
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/scenes
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/network
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/pool
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/render
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/update
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/block_update
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/task_manager
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/vulkan
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/window
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/perlin
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/world
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/world/entity
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/world/structures
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/network
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/network/client
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/network/server
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/network/shared
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/network/shared/packets
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/sound
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/event

		${CMAKE_CURRENT_SOURCE_DIR}/external/glm
		${CMAKE_CURRENT_SOURCE_DIR}/external/stb
		${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
		${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
		${CMAKE_CURRENT_SOURCE_DIR}/external/tracy/public/tracy
		${CMAKE_CURRENT_SOURCE_DIR}/external/freetype/freetype-2.13.3/include
		${CMAKE_CURRENT_SOURCE_DIR}/external/portaudio/include
)

target_link_libraries(${PROJECT_NAME} glfw vulkan dl pthread X11 Xxf86vm Xrandr Xi Tracy::TracyClient freetype portaudio_static)


add_executable(server)

target_compile_options(server
	PRIVATE
		-O3
		-g3
		# -fsanitize=thread
		-Wall
		-Wextra
		-Werror
		-Wpedantic
		-Wno-missing-braces

		# for tracy
		-Wno-missing-field-initializers
		-Wno-unused-parameter
		-Wno-empty-body
		-Wno-unused-result
		-Wno-c11-extensions
		-Wno-maybe-uninitialized
		-Wno-unused-variable
		-Wno-unused-function
)

target_link_options(server
	PRIVATE
		# -fsanitize=thread
)

target_sources(server
	PRIVATE
		src/tracy_globals.cpp
		src/server_main.cpp

		src/utils/logger.cpp

		src/app/perlin/Perlin.cpp

		src/app/DebugGui.cpp
		src/app/Settings.cpp
		src/app/scenes/Camera.cpp
		src/app/scenes/WorldScene.cpp
		src/app/threads/Status.cpp
		src/app/threads/render/RenderThread.cpp
		src/app/threads/render/CascadedShadowMapping.cpp
		src/app/threads/update/UpdateThread.cpp
		src/app/threads/pool/ThreadPool.cpp
		src/app/threads/pool/ThreadPoolAccessor.cpp
		src/app/threads/block_update/ServerBlockUpdateThread.cpp
		src/app/threads/network/NetworkThread.cpp
		src/app/threads/task_manager/ExecutorWorker.cpp
		src/app/threads/task_manager/Executor.cpp
		src/app/vulkan/VulkanAPI.cpp
		src/app/vulkan/VulkanMemoryAllocator.cpp
		src/app/vulkan/chunk_mesh.cpp
		src/app/vulkan/vk_helper.cpp
		src/app/vulkan/Image.cpp
		src/app/vulkan/Command.cpp
		src/app/vulkan/Descriptor.cpp
		src/app/vulkan/Swapchain.cpp
		src/app/vulkan/TextRenderer.cpp
		src/app/vulkan/Buffer.cpp
		src/app/vulkan/item_mesh.cpp
		src/app/window/window.cpp
		src/app/window/input.cpp
		src/app/world/Chunk.cpp
		src/app/world/entity/Player.cpp
		src/app/world/entity/HitBox.cpp
		src/app/world/entity/Mob.cpp
		src/app/world/ClientWorld.cpp
		src/app/world/ServerWorldTickets.cpp
		src/app/world/ServerWorldNetwork.cpp
		src/app/world/ServerWorldBlocks.cpp
		src/app/world/ServerWorld.cpp
		src/app/world/World.cpp
		src/app/world/WorldGenerator.cpp
		src/app/world/Block.cpp
		src/app/world/structures/Structures.cpp
		src/app/world/Item.cpp
		src/app/world/CreateMeshData.cpp
		src/app/network/client/Client.cpp
		src/app/network/client/ClientSocket.cpp
		src/app/network/client/ClientPacketHandler.cpp
		src/app/network/server/Server.cpp
		src/app/network/server/ServerSocket.cpp
		src/app/network/server/ServerPacketHandler.cpp
		src/app/network/shared/Connection.cpp
		src/app/network/shared/Poller.cpp
		src/app/network/shared/Socket.cpp
		src/app/network/shared/ConnectionSocket.cpp
		src/app/network/shared/PacketFactory.cpp
		src/app/network/shared/IncomingPacketList.cpp
		src/app/network/shared/packets/PingPacket.cpp
		src/app/network/shared/packets/BlockActionPacket.cpp
		src/app/network/shared/packets/IPacket.cpp
		src/app/network/shared/packets/DisconnectPacket.cpp
		src/app/network/shared/packets/PlayerConnectedPacket.cpp
		src/app/network/shared/packets/PlayerListPacket.cpp
		src/app/network/shared/packets/ChunkPacket.cpp
		src/app/network/shared/packets/ChunkRequestPacket.cpp
		src/app/network/shared/packets/ChunkUnloadPacket.cpp
		src/app/network/shared/packets/PlayerMovePacket.cpp
		src/app/network/shared/packets/ConnectionPacket.cpp
		src/app/sound/SoundEngine.cpp
		src/app/sound/AudioData.cpp
		src/app/event/AbstractEvent.cpp
		src/app/event/EventManager.cpp

		external/imgui/imgui.cpp
		external/imgui/imgui_draw.cpp
		external/imgui/imgui_widgets.cpp
		external/imgui/imgui_tables.cpp
		external/imgui/imgui_demo.cpp
		external/imgui/backends/imgui_impl_glfw.cpp
		external/imgui/backends/imgui_impl_vulkan.cpp

		external/tracy/public/TracyClient.cpp
)

# include directories (equivalent to -I)
target_include_directories(server
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/src/utils
		${CMAKE_CURRENT_SOURCE_DIR}/src/app
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/scenes
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/network
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/pool
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/render
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/update
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/block_update
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/threads/task_manager
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/vulkan
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/window
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/perlin
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/world
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/world/entity
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/world/structures
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/network
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/network/client
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/network/server
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/network/shared
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/network/shared/packets
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/sound
		${CMAKE_CURRENT_SOURCE_DIR}/src/app/event

		${CMAKE_CURRENT_SOURCE_DIR}/external/glm
		${CMAKE_CURRENT_SOURCE_DIR}/external/stb
		${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
		${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
		${CMAKE_CURRENT_SOURCE_DIR}/external/tracy/public/tracy
		${CMAKE_CURRENT_SOURCE_DIR}/external/freetype/freetype-2.13.3/include
		${CMAKE_CURRENT_SOURCE_DIR}/external/portaudio/include
)

target_link_libraries(server glfw vulkan dl pthread X11 Xxf86vm Xrandr Xi Tracy::TracyClient freetype portaudio_static)
