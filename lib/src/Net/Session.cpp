#include "Net/Session.h"
#include <algorithm>
#include <cstring>

namespace Net {

std::uint8_t PlayerInput::pack() const {
	std::uint8_t bits = 0;
	if (up) bits |= 1u;
	if (down) bits |= 2u;
	if (left) bits |= 4u;
	if (right) bits |= 8u;
	if (collect) bits |= 16u;
	if (selfDestruct) bits |= 32u;
	return bits;
}

PlayerInput PlayerInput::unpack(std::uint8_t bits) {
	PlayerInput in;
	in.up = (bits & 1u) != 0;
	in.down = (bits & 2u) != 0;
	in.left = (bits & 4u) != 0;
	in.right = (bits & 8u) != 0;
	in.collect = (bits & 16u) != 0;
	in.selfDestruct = (bits & 32u) != 0;
	return in;
}

namespace {
	constexpr float kDiscoverInterval = 0.5f;
	constexpr float kHostStale = 3.f;
	constexpr float kInputTimeout = 2.f;
}

void Session::resetTickState() {
	m_tick = 0;
	m_simBlocked = false;
	m_haveTick = false;
	m_sentInput = false;
	m_inputWait = 0.f;
	m_gotInput.fill(false);
	m_tickInputs = {};
	m_pendingTicks.clear();
}

void Session::poll(float dt) {
	if (m_udpBound) pollDiscover();

	if (m_role == Role::Host) {
		m_discoverTimer += dt;
		if (m_discoverTimer >= kDiscoverInterval) {
			m_discoverTimer = 0.f;
			broadcastDiscover();
		}
		acceptClients();
		pollPeers();
		if (m_playing && m_simBlocked)
			m_inputWait += dt;
	}
	else if (m_role == Role::Client) {
		pollPeers();
		if (m_playing)
			sendClientInput();
	}

	for (HostInfo& host : m_hosts) host.age += dt;
	m_hosts.erase(std::remove_if(m_hosts.begin(), m_hosts.end(),
		[](const HostInfo& h) { return h.age > kHostStale; }), m_hosts.end());
}

void Session::shutdown() {
	leave();
	if (m_udpBound) {
		m_udp.unbind();
		m_udpBound = false;
	}
	m_hosts.clear();
}

bool Session::host(const std::string& name) {
	leave();
	m_localName = name.empty() ? "PLAYER" : name;
	m_listener.setBlocking(false);
	if (m_listener.listen(GamePort) != sf::Socket::Status::Done)
		return false;

	if (!m_udpBound) {
		m_udp.setBlocking(false);
		if (m_udp.bind(DiscoverPort) != sf::Socket::Status::Done) {
			m_listener.close();
			return false;
		}
		m_udpBound = true;
	}

	m_role = Role::Host;
	m_localId = 0;
	m_names = { m_localName };
	m_playing = false;
	m_startReady = false;
	m_partyEnded = false;
	resetTickState();
	m_discoverTimer = kDiscoverInterval;
	broadcastDiscover();
	return true;
}

bool Session::join(const sf::IpAddress& address, unsigned short port, const std::string& name) {
	leave();
	m_localName = name.empty() ? "PLAYER" : name;
	auto peer = std::make_unique<Peer>();
	peer->socket.setBlocking(true);
	if (peer->socket.connect(address, port, sf::seconds(2.f)) != sf::Socket::Status::Done)
		return false;
	peer->socket.setBlocking(false);

	m_peers.push_back(std::move(peer));
	sf::Packet hello;
	hello << static_cast<std::uint8_t>(Msg::Hello) << m_localName;
	queueSend(*m_peers.back(), hello);

	m_role = Role::Client;
	m_localId = 0;
	m_names = { m_localName };
	m_playing = false;
	m_startReady = false;
	m_partyEnded = false;
	resetTickState();
	return true;
}

void Session::leave() {
	if (m_role == Role::None) return;
	sf::Packet leave;
	leave << static_cast<std::uint8_t>(Msg::Leave);
	if (m_role == Role::Client && !m_peers.empty())
		queueSend(*m_peers[0], leave);
	else if (m_role == Role::Host)
		sendAll(leave);

	for (auto& peer : m_peers)
		flushPeer(*peer);

	m_peers.clear();
	m_listener.close();
	m_role = Role::None;
	m_names.clear();
	m_playing = false;
	m_startReady = false;
	resetTickState();
}

void Session::startGame(int fileIndex, int caveNumber, std::uint32_t seed) {
	if (m_role != Role::Host) return;
	m_start.fileIndex = fileIndex;
	m_start.caveNumber = caveNumber;
	m_start.seed = seed;
	m_start.playerCount = playerCount();
	m_start.localId = 0;
	m_start.names = m_names;
	m_startReady = false;
	m_playing = true;
	resetTickState();

	sf::Packet packet;
	packet << static_cast<std::uint8_t>(Msg::Start)
		<< static_cast<std::int32_t>(fileIndex)
		<< static_cast<std::int32_t>(caveNumber)
		<< seed
		<< static_cast<std::uint8_t>(m_start.playerCount);
	for (const std::string& n : m_names) packet << n;
	sendAll(packet);
}

void Session::refreshHosts() {
	if (!m_udpBound) {
		m_udp.setBlocking(false);
		if (m_udp.bind(DiscoverPort) == sf::Socket::Status::Done)
			m_udpBound = true;
	}
}

const std::string& Session::playerName(int id) const {
	static const std::string fallback = "PLAYER";
	if (id < 0 || id >= static_cast<int>(m_names.size())) return fallback;
	return m_names[static_cast<size_t>(id)];
}

bool Session::consumeStart(StartInfo& out) {
	if (!m_startReady) return false;
	out = m_start;
	m_startReady = false;
	return true;
}

bool Session::consumePartyEnded() {
	if (!m_partyEnded) return false;
	m_partyEnded = false;
	return true;
}

void Session::sendClientInput() {
	if (m_role != Role::Client || m_peers.empty() || !m_playing || m_sentInput)
		return;
	sf::Packet packet;
	packet << static_cast<std::uint8_t>(Msg::Input) << m_tick << m_localInput.pack();
	queueSend(*m_peers[0], packet);
	m_sentInput = true;
}

void Session::submitLocalInput(const PlayerInput& input) {
	m_localInput = input;
	if (!m_playing) return;
	if (m_role == Role::Client)
		sendClientInput();
	else if (m_role == Role::Host) {
		m_tickInputs[0] = input;
		m_gotInput[0] = true;
	}
}

bool Session::tryBeginSimTick(std::array<PlayerInput, MaxPlayers>& out) {
	if (!m_playing || m_role == Role::None) {
		out = {};
		out[0] = m_localInput;
		m_simBlocked = false;
		return true;
	}

	if (m_role == Role::Host) {
		m_tickInputs[0] = m_localInput;
		m_gotInput[0] = true;
		if (!allInputsReady() && m_inputWait < kInputTimeout) {
			m_simBlocked = true;
			return false;
		}
		beginHostTick();
		out = m_tickInputs;
		m_gotInput.fill(false);
		m_inputWait = 0.f;
		m_simBlocked = false;
		return true;
	}

	sendClientInput();
	if (m_pendingTicks.empty()) {
		m_simBlocked = true;
		return false;
	}
	out = m_pendingTicks.front();
	m_pendingTicks.pop_front();
	m_sentInput = false;
	sendClientInput();
	m_simBlocked = false;
	return true;
}

void Session::broadcastDiscover() {
	if (!m_udpBound || m_playing) return;
	sf::Packet packet;
	packet << DiscoverMagic << GamePort
		<< static_cast<std::uint8_t>(playerCount())
		<< static_cast<std::uint8_t>(MaxPlayers)
		<< m_localName;
	static_cast<void>(m_udp.send(packet, sf::IpAddress::Broadcast, DiscoverPort));
}

void Session::pollDiscover() {
	sf::Packet packet;
	std::optional<sf::IpAddress> sender;
	unsigned short port = 0;
	while (m_udp.receive(packet, sender, port) == sf::Socket::Status::Done) {
		if (!sender) continue;
		std::uint32_t magic = 0;
		unsigned short gamePort = 0;
		std::uint8_t players = 0;
		std::uint8_t maxPlayers = 0;
		std::string name;
		if (!(packet >> magic >> gamePort >> players >> maxPlayers >> name)) continue;
		if (magic != DiscoverMagic) continue;
		if (m_role == Role::Host) continue;

		auto it = std::find_if(m_hosts.begin(), m_hosts.end(), [&](const HostInfo& h) {
			return h.address == *sender && h.port == gamePort;
		});
		if (it == m_hosts.end()) {
			HostInfo info;
			info.address = *sender;
			info.port = gamePort;
			info.name = name;
			info.players = players;
			info.maxPlayers = maxPlayers;
			info.age = 0.f;
			m_hosts.push_back(info);
		}
		else {
			it->name = name;
			it->players = players;
			it->maxPlayers = maxPlayers;
			it->age = 0.f;
		}
	}
}

void Session::acceptClients() {
	while (true) {
		auto peer = std::make_unique<Peer>();
		peer->socket.setBlocking(false);
		if (m_listener.accept(peer->socket) != sf::Socket::Status::Done)
			break;
		if (playerCount() >= MaxPlayers || m_playing) {
			peer->socket.disconnect();
			continue;
		}
		m_peers.push_back(std::move(peer));
	}
}

void Session::pollPeers() {
	for (int i = static_cast<int>(m_peers.size()) - 1; i >= 0; --i) {
		flushPeer(*m_peers[static_cast<size_t>(i)]);
		while (i < static_cast<int>(m_peers.size())) {
			sf::Packet packet;
			const sf::Socket::Status status = m_peers[static_cast<size_t>(i)]->socket.receive(packet);
			if (status == sf::Socket::Status::Done) {
				handlePacket(i, packet);
				continue;
			}
			if (status == sf::Socket::Status::Disconnected)
				dropPeer(i);
			break;
		}
	}
}

void Session::handlePacket(int peerIndex, sf::Packet& packet) {
	std::uint8_t type = 0;
	if (!(packet >> type)) return;
	auto msg = static_cast<Msg>(type);

	if (m_role == Role::Host) {
		switch (msg) {
		case Msg::Hello: {
			std::string name;
			if (!(packet >> name) || name.empty()) name = "PLAYER";
			m_peers[static_cast<size_t>(peerIndex)]->name = name;
			m_peers[static_cast<size_t>(peerIndex)]->named = true;
			m_names.clear();
			m_names.push_back(m_localName);
			for (const auto& peer : m_peers) {
				if (peer->named) m_names.push_back(peer->name);
			}
			sf::Packet welcome;
			welcome << static_cast<std::uint8_t>(Msg::Welcome)
				<< static_cast<std::uint8_t>(peerIndex + 1)
				<< static_cast<std::uint8_t>(m_names.size());
			for (const std::string& n : m_names) welcome << n;
			queueSend(*m_peers[static_cast<size_t>(peerIndex)], welcome);
			broadcastParty();
			break;
		}
		case Msg::Input: {
			std::uint32_t tick = 0;
			std::uint8_t bits = 0;
			if (!(packet >> tick >> bits)) break;
			const int id = peerIndex + 1;
			if (id < 0 || id >= MaxPlayers) break;
			if (tick == m_tick) {
				m_tickInputs[static_cast<size_t>(id)] = PlayerInput::unpack(bits);
				m_gotInput[static_cast<size_t>(id)] = true;
			}
			break;
		}
		case Msg::Leave:
			dropPeer(peerIndex);
			break;
		default:
			break;
		}
		return;
	}

	switch (msg) {
	case Msg::Welcome: {
		std::uint8_t localId = 0;
		std::uint8_t count = 0;
		if (!(packet >> localId >> count)) break;
		m_localId = localId;
		m_names.clear();
		for (std::uint8_t i = 0; i < count; ++i) {
			std::string n;
			packet >> n;
			m_names.push_back(n);
		}
		break;
	}
	case Msg::Party: {
		std::uint8_t count = 0;
		if (!(packet >> count)) break;
		m_names.clear();
		for (std::uint8_t i = 0; i < count; ++i) {
			std::string n;
			packet >> n;
			m_names.push_back(n);
		}
		break;
	}
	case Msg::Start: {
		std::int32_t fileIndex = 0;
		std::int32_t caveNumber = 0;
		std::uint32_t seed = 0;
		std::uint8_t count = 0;
		if (!(packet >> fileIndex >> caveNumber >> seed >> count)) break;
		m_start.fileIndex = fileIndex;
		m_start.caveNumber = caveNumber;
		m_start.seed = seed;
		m_start.playerCount = count;
		m_start.localId = m_localId;
		m_start.names.clear();
		for (std::uint8_t i = 0; i < count; ++i) {
			std::string n;
			packet >> n;
			m_start.names.push_back(n);
		}
		m_names = m_start.names;
		m_startReady = true;
		m_playing = true;
		resetTickState();
		m_sentInput = false;
		break;
	}
	case Msg::Tick: {
		std::uint32_t tick = 0;
		std::uint8_t count = 0;
		if (!(packet >> tick >> count)) break;
		for (std::uint8_t i = 0; i < count && i < MaxPlayers; ++i) {
			std::uint8_t bits = 0;
			packet >> bits;
			m_tickInputs[i] = PlayerInput::unpack(bits);
		}
		m_tick = tick + 1;
		m_pendingTicks.push_back(m_tickInputs);
		m_haveTick = true;
		m_simBlocked = false;
		break;
	}
	case Msg::Leave:
		leave();
		m_partyEnded = true;
		break;
	default:
		break;
	}
}

void Session::sendAll(sf::Packet packet) {
	for (auto& peer : m_peers)
		queueSend(*peer, packet);
}

void Session::queueSend(Peer& peer, sf::Packet packet) {
	peer.outbound.push_back(std::move(packet));
	flushPeer(peer);
}

void Session::flushPeer(Peer& peer) {
	while (!peer.outbound.empty()) {
		sf::Socket::Status status = peer.socket.send(peer.outbound.front());
		if (status == sf::Socket::Status::Partial)
			continue;
		if (status == sf::Socket::Status::Done) {
			peer.outbound.pop_front();
			continue;
		}
		break;
	}
}

void Session::broadcastParty() {
	sf::Packet packet;
	packet << static_cast<std::uint8_t>(Msg::Party) << static_cast<std::uint8_t>(m_names.size());
	for (const std::string& n : m_names) packet << n;
	sendAll(packet);
}

void Session::dropPeer(int index) {
	if (index < 0 || index >= static_cast<int>(m_peers.size())) return;
	m_peers.erase(m_peers.begin() + index);
	if (m_role == Role::Host) {
		m_names.clear();
		m_names.push_back(m_localName);
		for (const auto& peer : m_peers) {
			if (peer->named) m_names.push_back(peer->name);
		}
		broadcastParty();
	}
	else {
		m_role = Role::None;
		m_playing = false;
		m_partyEnded = true;
		m_peers.clear();
		resetTickState();
	}
}

void Session::beginHostTick() {
	m_tickInputs[0] = m_localInput;
	m_gotInput[0] = true;
	for (int i = 1; i < playerCount(); ++i) {
		if (!m_gotInput[static_cast<size_t>(i)])
			m_tickInputs[static_cast<size_t>(i)] = {};
	}
	sf::Packet packet;
	packet << static_cast<std::uint8_t>(Msg::Tick) << m_tick << static_cast<std::uint8_t>(playerCount());
	for (int i = 0; i < playerCount(); ++i)
		packet << m_tickInputs[static_cast<size_t>(i)].pack();
	sendAll(packet);
	m_tick += 1;
}

bool Session::allInputsReady() const {
	for (int i = 0; i < playerCount(); ++i) {
		if (!m_gotInput[static_cast<size_t>(i)]) return false;
	}
	return true;
}

}
