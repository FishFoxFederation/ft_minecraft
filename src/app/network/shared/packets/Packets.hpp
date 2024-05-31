#pragma once

#include "PlayerMovePacket.hpp"
#include "PlayerConnectedPacket.hpp"
#include "ConnectionPacket.hpp"
#include "Client.hpp"
#include "Server.hpp"


/**
 * C > S : ConnectionPacket
 * S > all C : PlayerConnectedPacket
 * 
 * C > S : PlayerMovePacket
 * S > all C : PlayerMovePacket
 * 
 */
