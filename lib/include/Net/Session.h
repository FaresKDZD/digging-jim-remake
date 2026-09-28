#pragma once
#include <SFML/Network.hpp>
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace Net {

    constexpr int MaxPlayers = 4;
    constexpr unsigned short DiscoverPort = 27451;
    constexpr unsigned short GamePort = 27452;
    constexpr std::uint32_t DiscoverMagic = 0x444A494Du; // DJIM

    struct PlayerInput {
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        bool collect = false;
        bool selfDestruct = false;

        std::uint8_t pack() const;
        static PlayerInput unpack(std::uint8_t bits);
    };

    struct HostInfo {
        sf::IpAddress address = sf::IpAddress::Any;
        unsigned short port = GamePort;
        std::string name;
        int players = 1;
        int maxPlayers = MaxPlayers;
        float age = 0.f;
    };

    struct StartInfo {
        int fileIndex = 0;
        int caveNumber = 1;
        std::uint32_t seed = 0;
        int playerCount = 1;
        int localId = 0;
        std::vector<std::string> names;
    };

    enum class Role {
        None,
        Host,
        Client
    };

    class Session {
    public:
        void poll(float dt);
        void shutdown();

        bool host(const std::string& name);
        bool join(const sf::IpAddress& address, unsigned short port, const std::string& name);
        void leave();
        void startGame(int fileIndex, int caveNumber, std::uint32_t seed);

        void refreshHosts();
        const std::vector<HostInfo>& discoveredHosts() const { return m_hosts; }

        Role role() const { return m_role; }
        bool inParty() const { return m_role != Role::None; }
        bool isHost() const { return m_role == Role::Host; }
        bool playing() const { return m_playing; }
        bool consumeStart(StartInfo& out);
        bool consumePartyEnded();
        bool waitingForTick() const { return m_playing && m_simBlocked; }

        int localId() const { return m_localId; }
        int playerCount() const { return static_cast<int>(m_names.size()); }
        const std::vector<std::string>& names() const { return m_names; }
        const std::string& playerName(int id) const;

        void submitLocalInput(const PlayerInput& input);
        bool tryBeginSimTick(std::array<PlayerInput, MaxPlayers>& out);

        void setPlaying(bool playing) { m_playing = playing; }

    private:
        enum class Msg : std::uint8_t {
            Hello = 1,
            Welcome,
            Party,
            Start,
            Input,
            Tick,
            Leave
        };

        struct Peer {
            sf::TcpSocket socket;
            std::string name;
            bool named = false;
            std::deque<sf::Packet> outbound;
        };

        void broadcastDiscover();
        void pollDiscover();
        void acceptClients();
        void pollPeers();
        void handlePacket(int peerIndex, sf::Packet& packet);
        void sendAll(sf::Packet packet);
        void queueSend(Peer& peer, sf::Packet packet);
        void flushPeer(Peer& peer);
        void broadcastParty();
        void dropPeer(int index);
        void beginHostTick();
        bool allInputsReady() const;
        void resetTickState();
        void sendClientInput();

        Role m_role = Role::None;
        std::string m_localName = "PLAYER";
        int m_localId = 0;
        std::vector<std::string> m_names;
        bool m_playing = false;
        bool m_startReady = false;
        bool m_partyEnded = false;
        StartInfo m_start;

        sf::TcpListener m_listener;
        sf::UdpSocket m_udp;
        std::vector<std::unique_ptr<Peer>> m_peers;
        std::vector<HostInfo> m_hosts;

        std::uint32_t m_tick = 0;
        bool m_simBlocked = false;
        bool m_haveTick = false;
        bool m_sentInput = false;
        PlayerInput m_localInput;
        std::array<PlayerInput, MaxPlayers> m_tickInputs{};
        std::array<bool, MaxPlayers> m_gotInput{};
        std::deque<std::array<PlayerInput, MaxPlayers>> m_pendingTicks;
        float m_inputWait = 0.f;
        float m_discoverTimer = 0.f;
        bool m_udpBound = false;
    };
}
