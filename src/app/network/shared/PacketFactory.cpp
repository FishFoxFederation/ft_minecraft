#include "PacketFactory.hpp"

PacketFactory::PacketFactory()
{
	m_packets.insert(std::make_pair(IPacket::Type::CONNECTION, std::make_shared<ConnectionPacket>()));
	m_packets.insert(std::make_pair(IPacket::Type::PLAYER_CONNECTED, std::make_shared<PlayerConnectedPacket>()));
	m_packets.insert(std::make_pair(IPacket::Type::PLAYER_MOVE, std::make_shared<PlayerMovePacket>()));
	m_packets.insert(std::make_pair(IPacket::Type::DISCONNECT, std::make_shared<DisconnectPacket>()));
	m_packets.insert(std::make_pair(IPacket::Type::BLOCK_ACTION, std::make_shared<BlockActionPacket>()));
	m_packets.insert(std::make_pair(IPacket::Type::PING, std::make_shared<PingPacket>()));
	m_packets.insert(std::make_pair(IPacket::Type::PLAYER_LIST, std::make_shared<PlayerListPacket>()));
	m_packets.insert(std::make_pair(IPacket::Type::CHUNK, std::make_shared<ChunkPacket>()));
	m_packets.insert(std::make_pair(IPacket::Type::CHUNK_REQUEST, std::make_shared<ChunkRequestPacket>()));
	m_packets.insert(std::make_pair(IPacket::Type::CHUNK_UNLOAD, std::make_shared<ChunkUnloadPacket>()));
	m_packets.insert(std::make_pair(IPacket::Type::LOAD_DISTANCE, std::make_shared<LoadDistancePacket>()));


	IPacket::Type type = IPacket::Type::ENUM_MIN;
	// If an exception brought you here you most likely are trying to implement a new packet
	//you should check that all packet type that are present in the IPacket::Type enum are present inside the
	// paket factory
	while (type != IPacket::Type::ENUM_MAX)
	{
		if (!m_packets.contains(type))
		{
			LOG_ERROR("PacketFactory::PacketFactory: " << "Missing packet type: " << static_cast<int>(type));
			throw std::runtime_error("PacketFactory::PacketFactory: Missing packet type");
		}
		type = static_cast<IPacket::Type>(static_cast<uint32_t>(type) + 1);
	}
}

PacketFactory::~PacketFactory()
{
}

/**
 * @details This private method is used to read a connection buffer and extract information about a potential packet,
 * it will see if it can read the packet type,
 * then if the packet has a dynamic size ( meaning the size is not fixed so it can vary between packets of the same type )
 * it will read the size of the packet that has been encoded in the buffer right after the packet type
 */
PacketFactory::packetInfo PacketFactory::getPacketInfo(const uint8_t * buffer, const size_t & size) const
{
	ZoneScoped;
	packetInfo retInfo = {false, IPacket::Type::ENUM_MAX, 0};

	// Check if the buffer is big enough to contain the packet type
	if (size < sizeof(IPacket::Type))
		return retInfo;

	//extract the packet type
	uint32_t packet_type = 0;
	memcpy(&packet_type, buffer, sizeof(IPacket::Type));

	//check if the packet type is valid
	if (packet_type >= static_cast<uint32_t>(IPacket::Type::ENUM_MAX))
	{
		LOG_ERROR("PacketFactory::getPacketType: " << "Invalid packet type: " << packet_type);
		return retInfo;
	}

	IPacket::Type type = static_cast<IPacket::Type>(packet_type);

	size_t packet_size = 0;

	//extract the packet size if the packet has a dynamic size
	if (m_packets.at(type)->HasDynamicSize())
	{
		if (size < sizeof(IPacket::Type) + sizeof(size_t))
			return retInfo;

		memcpy(&packet_size, buffer + sizeof(IPacket::Type), sizeof(size_t));
	}
	else
		packet_size = m_packets.at(type)->Size();

	//check if the buffer is big enough to contain the packet
	if (size < packet_size)
		return retInfo;

	retInfo.complete = true;
	retInfo.type = type;
	retInfo.size = packet_size;
	return retInfo;
}

/**
 * @details calls getPacketInfo to get information about a potential packet in the connection's buffer
 * if the packet is complete, instanciates a new packet of the correct type and extracts the message from the connection
 * all buffer manipulation is done in connection and IPacket classes
 */
std::pair<bool, std::shared_ptr<IPacket>> PacketFactory::extractPacket(Connection & connection)
{
	ZoneScoped;
	PacketFactory::packetInfo packetRet = getPacketInfo(connection.getReadBufferPtr(), connection.getReadBufferSize());

	std::pair<bool, std::shared_ptr<IPacket>> ret = std::make_pair(false, nullptr);
	if (packetRet.complete)
	{
		std::shared_ptr<IPacket> packet = m_packets.at(packetRet.type)->Clone();
		packet->ExtractMessage(connection);
		ret.first = true;
		ret.second = packet;
	}

	return ret;
}

PacketFactory& PacketFactory::GetInstance()
{
	static PacketFactory instance;
	return instance;
}
