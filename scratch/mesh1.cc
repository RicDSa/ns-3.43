/*
 * Copyright (c) 2008,2009 IITP RAS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Kirill Andreev <andreev@iitp.ru>
 *
 *
 * By default this script creates m_xSize * m_ySize square grid topology with
 * IEEE802.11s stack installed at each node with peering management
 * and HWMP protocol.
 * The side of the square cell is defined by m_step parameter.
 * When topology is created, UDP ping is installed to opposite corners
 * by diagonals. packet size of the UDP ping and interval between two
 * successive packets is configurable.
 *
 *  m_xSize * step
 *  |<--------->|
 *   step
 *  |<--->|
 *  * --- * --- * <---Ping sink  _
 *  | \   |   / |                ^
 *  |   \ | /   |                |
 *  * --- * --- * m_ySize * step |
 *  |   / | \   |                |
 *  | /   |   \ |                |
 *  * --- * --- *                _
 *  ^ Ping source
 *
 * By varying m_xSize and m_ySize, one can configure the route that is used.
 * When the inter-nodal distance is small, the source can reach the sink
 * directly.  When the inter-nodal distance is intermediate, the route
 * selected is diagonal (two hop).  When the inter-nodal distance is a bit
 * larger, the diagonals cannot be used and a four-hop route is selected.
 * When the distance is a bit larger, the packets will fail to reach even the
 * adjacent nodes.
 *
 * As of ns-3.36 release, with default configuration (mesh uses Wi-Fi 802.11a
 * standard and the ArfWifiManager rate control by default), the maximum
 * range is roughly 50m.  The default step size in this program is set to 50m,
 * so any mesh packets in the above diagram depiction will not be received
 * successfully on the diagonal hops between two nodes but only on the
 * horizontal and vertical hops.  If the step size is reduced to 35m, then
 * the shortest path will be on the diagonal hops.  If the step size is reduced
 * to 17m or less, then the source will be able to reach the sink directly
 * without any mesh hops (for the default 3x3 mesh depicted above).
 *
 * The position allocator will lay out the nodes in the following order
 * (corresponding to Node ID and to the diagram above):
 *
 * 6 - 7 - 8
 * |   |   |
 * 3 - 4 - 5
 * |   |   |
 * 0 - 1 - 2
 *
 *  See also MeshTest::Configure to read more about configurable
 *  parameters.
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-global-routing.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/mesh-helper.h"
#include "ns3/mesh-module.h"
#include "ns3/mesh-point-device.h"
#include "ns3/mesh-wifi-interface-mac.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/packet.h"
#include "ns3/peer-link-frame.h"
#include "ns3/peer-link.h"
#include "ns3/peer-management-protocol.h"
#include "ns3/yans-wifi-helper.h"

#include <fstream>
#include <iostream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MeshExample");

// Declaring these variables outside of main() for use in trace sinks
uint32_t g_TxCount = 0;                                //!< Rx packet counter.
uint32_t g_RxCount = 0;                                //!< Tx packet counter.
Ipv4Address multicastGroup = Ipv4Address("225.1.2.5"); // Multicast group address
uint16_t multicastPort = 8080;                         // Multicast port
// List of nodes in the multicast group
std::set<uint32_t> multicastGroupNodes = {1, 3, 5, 7};

// Global set to track received packet IDs
std::set<uint32_t> receivedPacketIds;

void
SendMulticastPacket(Ptr<Socket> socket,
                    uint32_t packetSize,
                    Ipv4Address multicastGroup,
                    InetSocketAddress remote)
{
    Ptr<Node> node = socket->GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    Ipv4Address sourceAddress = ipv4->GetAddress(1, 0).GetLocal();

    Ptr<Packet> packet = Create<Packet>(packetSize);

    std::cout << "Sending multicast packet from " << sourceAddress << " at "
              << Simulator::Now().GetSeconds() << "s" << std::endl;

    uint32_t bytesSent = socket->SendTo(packet, 0, remote);

    if (bytesSent == packetSize)
    {
        std::cout << "Packet sent successfully" << std::endl;
    }
    else
    {
        std::cout << "Packet send failed. Sent " << bytesSent << " out of " << packetSize
                  << " bytes" << std::endl;
    }
    /* Simulator::Schedule(Seconds(1.0),
                        &SendMulticastPacket,
                        socket,
                        packetSize,
                        multicastGroup,
                        remote); */
    g_TxCount++;
}

void
ReceivePacket(Ptr<Socket> socket)
{
    std::cout << "Received packet at Node: " << socket->GetNode()->GetId() << std::endl;
    Ptr<Node> node = socket->GetNode(); // Get the receiving node
    uint32_t nodeId = node->GetId();    // Get the node ID
    Ptr<Packet> packet;
    Address from;

    // Retrieve the packet and the sender's address
    while ((packet = socket->RecvFrom(from)))
    {
        // Get the size of the packet
        uint32_t packetSize = packet->GetSize();

        // Check if the sender address is an IPv4 address
        InetSocketAddress senderAddress = InetSocketAddress::ConvertFrom(from);
        Ipv4Address senderIp = senderAddress.GetIpv4();

        // Get the receiver node's IP address
        Ptr<Node> node = socket->GetNode();
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ipv4Address receiverIp = ipv4->GetAddress(1, 0).GetLocal();

        // Print packet information
        std::cout << "Received packet of size " << packetSize << " from " << senderIp << " to "
                  << receiverIp << " at " << Simulator::Now().GetSeconds() << "s" << std::endl;

        g_RxCount++;
        static std::set<std::string> received;
        std::ostringstream oss;
        oss << senderIp; // senderIp is of type Ipv4Address
        std::string ipStr = oss.str();

        std::string pktId =
            ipStr + std::to_string(Simulator::Now().GetMicroSeconds());
        if (received.find(pktId) != received.end())
        {
            std::cout << "Packet already received from " << senderIp << std::endl;
            return; // Already received
        }
        received.insert(pktId);
    }
}

// Função que itera por todos os nós e imprime as métricas do HWMP
void
PrintAllHwmpMetrics()
{
    NS_LOG_UNCOND("===== Iniciando impressão das métricas HWMP para todos os nós =====");
    // Percorre todos os nós
    for (uint32_t i = 0; i < ns3::NodeList::GetNNodes(); i++)
    {
        ns3::Ptr<ns3::Node> node = ns3::NodeList::GetNode(i);
        ns3::Ptr<ns3::Ipv4> ipv4 = node->GetObject<ns3::Ipv4>();
        ns3::Ipv4Address ipAddr = ipv4->GetAddress(1, 0).GetLocal();
        NS_LOG_UNCOND("----- Nó " << node->GetId() << " (IP: " << ipAddr << ") -----");
        // Percorre todos os dispositivos do nó
        for (uint32_t j = 0; j < node->GetNDevices(); j++)
        {
            ns3::Ptr<ns3::NetDevice> dev = node->GetDevice(j);
            // Tenta fazer o cast para MeshPointDevice
            ns3::Ptr<ns3::MeshPointDevice> mp = dev->GetObject<ns3::MeshPointDevice>();
            if (mp)
            {
                // Obtém o protocolo de roteamento instalado neste MeshPointDevice
                ns3::Ptr<ns3::dot11s::HwmpProtocol> hwmp =
                    mp->GetRoutingProtocol()->GetObject<ns3::dot11s::HwmpProtocol>();
                if (hwmp)
                {
                    NS_LOG_UNCOND("Métricas para o MeshPointDevice deste nó:");
                    //hwmp->PrintParacodeMetrics();
                    //hwmp->PrintFirstReceivedTtl();
                }
            }
        }
    }
    NS_LOG_UNCOND("===== Fim da impressão das métricas HWMP =====");
}

/* void CheckPeerings()
{
    for (uint32_t nodeId = 0; nodeId < NodeList::GetNNodes(); ++nodeId)
{
    Ptr<Node> node = NodeList::GetNode(nodeId);
    Ptr<MeshPointDevice> mp = nullptr;

    // Find the MeshPointDevice on this node
    for (uint32_t d = 0; d < node->GetNDevices(); ++d)
    {
        mp = node->GetDevice(d)->GetObject<MeshPointDevice>();
        if (mp != nullptr)
            break;
    }

    if (mp == nullptr)
    {
        std::cout << "Node " << nodeId << ": No MeshPointDevice found.\n";
        continue;
    }

    std::vector<Mac48Address> ifaceAddrs = mp->GetInterfaceAddresses();
uint32_t ifIndex = 0;

for (const auto& addr : ifaceAddrs)
{
    Ptr<NetDevice> iface = mp->GetInterface(ifIndex);
    if (!iface)
    {
        std::cout << "  Skipping invalid interface index: " << ifIndex << "\n";
        ++ifIndex;
        continue;
    }

    Ptr<MeshWifiInterfaceMac> mac = iface->GetObject<MeshWifiInterfaceMac>();
    if (!mac)
    {
        std::cout << "  Interface " << ifIndex << ": MAC not found.\n";
        ++ifIndex;
        continue;
    }

    Ptr<dot11s::PeerManagementProtocol> pmp = mac->GetObject<dot11s::PeerManagementProtocol>();
    if (!pmp)
    {
        std::cout << "  Interface " << ifIndex << ": PeerManagementProtocol not found.\n";
        ++ifIndex;
        continue;
    }

    std::vector<Ptr<dot11s::PeerLink>> links = pmp->GetPeerLinks();
    std::cout << "  Interface " << ifIndex << ": " << links.size() << " established peerings.\n";

    for (const auto& link : links)
    {
        Mac48Address peerAddr = link->GetPeerAddress();
        std::cout << "    ↳ Peered with: " << peerAddr << std::endl;
    }

    ++ifIndex;
}

}

std::cout << "===================================\n\n";

}
 */

/* void
PrintEstablishedPeers()
{
    NS_LOG_UNCOND("===== Iniciando impressão dos peers estabelecidos =====");
    for (uint32_t nodeId = 0; nodeId < NodeList::GetNNodes(); ++nodeId)
    {
        std ::cout << "Node ID: " << nodeId << std::endl;
        Ptr<Node> node = NodeList::GetNode(nodeId);

        for (uint32_t devId = 0; devId < node->GetNDevices(); ++devId)
        {
            std ::cout << "Device ID: " << devId << std::endl;
            Ptr<NetDevice> dev = node->GetDevice(devId);

            std::cout << "Device: " << dev << std::endl;
            Ptr<MeshWifiInterfaceMac> mac = dev->GetObject<MeshWifiInterfaceMac>();
            std ::cout << "Mac: " << mac << std::endl;
            if (!mac)
                continue;

            Ptr<ns3::dot11s::PeerManagementProtocol> pmp =
                mac->GetObject<ns3::dot11s::PeerManagementProtocol>();
            if (!pmp)
                continue;

            std::set<Mac48Address> peers = pmp->GetEstablishedPeerAddresses();
            for (const auto& peer : peers)
            {
                std::cout << "Node " << nodeId << " has peer: " << peer << std::endl;
            }
        }
    }
}

 */
/**
 * \ingroup mesh
 * \brief MeshTest class
 */
class MeshTest
{
  public:
    /// Init test
    MeshTest();
    /**
     * Configure test from command line arguments
     *
     * \param argc command line argument count
     * \param argv command line arguments
     */
    void Configure(int argc, char** argv);
    /**
     * Run test
     * \returns the test status
     */
    int Run();

  private:
    int m_xSize;             ///< X size
    int m_ySize;             ///< Y size
    double m_step;           ///< step
    double m_randomStart;    ///< random start
    double m_totalTime;      ///< total time
    double m_packetInterval; ///< packet interval
    uint16_t m_packetSize;   ///< packet size
    uint32_t m_nIfaces;      ///< number interfaces
    bool m_chan;             ///< channel
    bool m_pcap;             ///< PCAP
    bool m_ascii;            ///< ASCII
    std::string m_stack;     ///< stack
    std::string m_root;      ///< root
    /// List of network nodes
    NodeContainer nodes;
    /// List of all mesh point devices
    NetDeviceContainer meshDevices;
    /// Addresses of interfaces:
    Ipv4InterfaceContainer interfaces;
    /// MeshHelper. Report is not static methods
    MeshHelper mesh;

  private:
    /// Create nodes and setup their mobility
    void CreateNodes();
    /// Install internet m_stack on nodes
    void InstallInternetStack();
    /// Install applications
    void InstallApplication();
    /// Print mesh devices diagnostics
    void Report();
};

MeshTest::MeshTest()
    : m_xSize(3),
      m_ySize(1),
      m_step(50.0),
      m_randomStart(0.1),
      m_totalTime(10.0),
      m_packetInterval(1),
      m_packetSize(1024),
      m_nIfaces(1),
      m_chan(true),
      m_pcap(true),
      m_ascii(true),
      m_stack("ns3::Dot11sStack"),
      m_root("ff:ff:ff:ff:ff:ff")
{
}

void
MeshTest::Configure(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.AddValue("x-size", "Number of nodes in a row grid", m_xSize);
    cmd.AddValue("y-size", "Number of rows in a grid", m_ySize);
    cmd.AddValue("step", "Size of edge in our grid (meters)", m_step);
    // Avoid starting all mesh nodes at the same time (beacons may collide)
    cmd.AddValue("start", "Maximum random start delay for beacon jitter (sec)", m_randomStart);
    cmd.AddValue("time", "Simulation time (sec)", m_totalTime);
    cmd.AddValue("packet-interval", "Interval between packets in UDP ping (sec)", m_packetInterval);
    cmd.AddValue("packet-size", "Size of packets in UDP ping (bytes)", m_packetSize);
    cmd.AddValue("interfaces", "Number of radio interfaces used by each mesh point", m_nIfaces);
    cmd.AddValue("channels", "Use different frequency channels for different interfaces", m_chan);
    cmd.AddValue("pcap", "Enable PCAP traces on interfaces", m_pcap);
    cmd.AddValue("ascii", "Enable Ascii traces on interfaces", m_ascii);
    cmd.AddValue("stack", "Type of protocol stack. ns3::Dot11sStack by default", m_stack);
    cmd.AddValue("root", "Mac address of root mesh point in HWMP", m_root);

    cmd.Parse(argc, argv);
    NS_LOG_DEBUG("Grid:" << m_xSize << "*" << m_ySize);
    NS_LOG_DEBUG("Simulation time: " << m_totalTime << " s");
    if (m_ascii)
    {
        PacketMetadata::Enable();
    }
}

void
MeshTest::CreateNodes()
{
    nodes.Create(m_xSize * m_ySize);
    std::cout << "Number of nodes created: " << nodes.GetN() << std::endl;

    // Configure YansWifiChannel
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    /*
     * Create mesh helper and set stack installer to it
     * Stack installer creates all needed protocols and install them to
     * mesh point device
     */
    mesh = MeshHelper::Default();
    if (!Mac48Address(m_root.c_str()).IsBroadcast())
    {
        mesh.SetStackInstaller(m_stack, "Root", Mac48AddressValue(Mac48Address(m_root.c_str())));
    }
    else
    {
        // If root is not set, we do not use "Root" attribute, because it
        // is specified only for 11s
        mesh.SetStackInstaller(m_stack);
    }
    if (m_chan)
    {
        mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    }
    else
    {
        mesh.SetSpreadInterfaceChannels(MeshHelper::ZERO_CHANNEL);
    }
    mesh.SetMacType("RandomStart", TimeValue(Seconds(m_randomStart)));
    // Set number of interfaces - default is single-interface mesh point
    mesh.SetNumberOfInterfaces(m_nIfaces);

    // Install protocols and return container if MeshPointDevices
    meshDevices = mesh.Install(wifiPhy, nodes);
    // AssignStreams can optionally be used to control random variable streams
    mesh.AssignStreams(meshDevices, 0);

    /* // Install mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Manually set positions
    Ptr<MobilityModel> mob;
    mob = nodes.Get(0)->GetObject<MobilityModel>();
    mob->SetPosition(Vector(0.0, 0.0, 0.0));

    mob = nodes.Get(1)->GetObject<MobilityModel>();
    mob->SetPosition(Vector(10.0, 0.0, 0.0));

    mob = nodes.Get(2)->GetObject<MobilityModel>();
    mob->SetPosition(Vector(20.0, 5.0, 0.0));

    mob = nodes.Get(3)->GetObject<MobilityModel>();
    mob->SetPosition(Vector(5.0, 15.0, 0.0));
 */
   /*  mob = nodes.Get(4)->GetObject<MobilityModel>();
    mob->SetPosition(Vector(15.0, 10.0, 0.0)); */
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        // Use ListPositionAllocator to manually set positions
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

        positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // Node 0
        positionAlloc->Add(Vector(10.0, 0.0, 0.0));  // Node 1
        positionAlloc->Add(Vector(20.0, 5.0, 0.0));  // Node 2
        positionAlloc->Add(Vector(5.0, 15.0, 0.0));  // Node 3
        positionAlloc->Add(Vector(15.0, 10.0, 0.0)); // Node 4
        positionAlloc->Add(Vector(5.0, 10.0, 0.0)); // Node 5
        positionAlloc->Add(Vector(10.0, 5.0, 0.0)); // Node 6

     
        mobility.SetPositionAllocator(positionAlloc);
        
        mobility.Install(nodes); 
    if (m_pcap)
    {
        wifiPhy.EnablePcapAll(
            std::string("/home/ricardosa/ns-allinone-3.43/ns-3.43/scratch/meshtrace1/mp"));
    }
    if (m_ascii)
    {
        AsciiTraceHelper ascii;
        wifiPhy.EnableAsciiAll(ascii.CreateFileStream("mesh.tr"));
    }

    // Now back‐patch the device pointer into your HWMP object:
    for (NetDeviceContainer::Iterator it = meshDevices.Begin(); it != meshDevices.End(); ++it)
    {
        Ptr<NetDevice> dev = *it;

        Ptr<MeshPointDevice> mpd = dev->GetObject<MeshPointDevice>();
        Ptr<ns3::dot11s::HwmpProtocol> hwmp = mpd->GetObject<ns3::dot11s::HwmpProtocol>();
        hwmp->SetDevice(mpd);

        hwmp->StartLinkMonitor(Seconds(7));
    }
}

void
MeshTest::InstallInternetStack()
{
    InternetStackHelper internetStack;

    internetStack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces = address.Assign(meshDevices);

    //  Create a multicast group

    Ipv4StaticRoutingHelper multicastRoutingHelper;

    Ptr<Node> sender = nodes.Get(0);
    Ptr<NetDevice> senderIf = meshDevices.Get(0);
    multicastRoutingHelper.SetDefaultMulticastRoute(sender, senderIf);
}

void
MeshTest::InstallApplication()
{
    // Set up a multicast receiver on the node
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        // Check if the node is in the multicast group
        if (multicastGroupNodes.find(i) != multicastGroupNodes.end())
        {
            Ptr<Socket> recvSink =
                Socket::CreateSocket(nodes.Get(i), UdpSocketFactory::GetTypeId());
            // Define the IP address and port for the socket to listen on
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), multicastPort);

            // Allow the socket to receive broadcast packets
            recvSink->SetAllowBroadcast(true);
            // Set the callback function to be called whenever the socket receives a packet
            recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

            // Bind the socket to the configured address and port
            recvSink->Bind(local);

            std::cout << "Setting up multicast receiver on node " << i << std::endl;
        }
        else
        {
            std::cout << "Node " << i << " is not in the multicast group. Skipping receiver setup."
                      << std::endl;
        }
    }

    // Set up a multicast sender on the first node
    // Obtém o objeto Ipv4 do nó 0
    Ptr<Ipv4> ipv4 = nodes.Get(0)->GetObject<Ipv4>();

    // Obtém o endereço IP da interface 1 do nó 0
    Ipv4Address ipAddress = ipv4->GetAddress(1, 0).GetLocal();

    Ptr<Socket> source = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    // Liga o socket ao endereço IP "10.1.1.1" e à porta 8080 no emissor.
    source->Bind(InetSocketAddress(ipAddress, multicastPort));
    // Define o endereço de destino do multicast (grupo multicast e porta)
    InetSocketAddress remote = InetSocketAddress(multicastGroup, multicastPort);
    // Permite que o socket envie pacotes em broadcast
    source->SetAllowBroadcast(true);
    // source->SetIpTtl(10);
    //  Estabelece a conexão com o grupo multicast e a porta
    source->Connect(remote);

    /*     Simulator::ScheduleWithContext(source->GetNode()->GetId(),
                                       Seconds(1.0),
                                       &SendMulticastPacket,
                                       source,
                                       m_packetSize,
                                       multicastGroup,
                                       remote); */

    Simulator::Schedule(Seconds(1.0), // Send after 1 second
                        &SendMulticastPacket,
                        source,
                        m_packetSize,
                        multicastGroup,
                        remote);
    /*
        Simulator::Schedule(Seconds(3.0), // Send after 1 second
                            &SendMulticastPacket,
                            source,
                            m_packetSize,
                            multicastGroup,
                            remote);
     */
    /*  Simulator::Schedule(Seconds(3.0), // Send after 3 second
                         &SendMulticastPacket,
                         source,
                         m_packetSize,
                         multicastGroup,
                         remote);  */
}

int
MeshTest::Run()
{
    // LogComponentEnable("UdpSocketImpl", LOG_LEVEL_ALL);
    // LogComponentEnable("UdpL4Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("Ipv4L3Protocol", LOG_LEVEL_ALL);
    // LogComponentEnable("HwmpProtocol", LOG_LEVEL_ALL);
    // LogComponentEnable("MeshPointDevice", LOG_LEVEL_ALL);
    // LogComponentEnable("MeshWifiInterfaceMac", LOG_LEVEL_ALL);
    // LogComponentEnable("YansWifiPhy", LOG_LEVEL_ALL);
    // LogComponentEnable("PeerManagementProtocol", LOG_LEVEL_ALL);
     LogComponentEnable("HwmpProtocolMac", LOG_LEVEL_ALL);
    LogComponentEnable("MeshWifiInterfaceMac", LOG_LEVEL_ALL);

    CreateNodes();
    InstallInternetStack();
    InstallApplication();

    AnimationInterface anim("mesh1.xml");

    Simulator::Schedule(Seconds(m_totalTime), &MeshTest::Report, this);
    Simulator::Stop(Seconds(m_totalTime + 2));

    // Simulator::Schedule(Seconds(10.0), &CheckPeerings);

    // PrintAllHwmpMetrics();

    Simulator::Run();
    // PrintEstablishedPeers();
    PrintAllHwmpMetrics();
    //   CheckPeerings();

    Simulator::Destroy();

    std::cout << "UDP echo packets enviados: " << g_TxCount << ", recebidos: " << g_RxCount
              << std::endl;
    return 0;
}

void
MeshTest::Report()
{
    // std::cout << "Relatório da simulação:" << std::endl;
    /*
        // Obtém o nó 0 (ou outro nó relevante)
        Ptr<Node> node = nodes.Get(0);

        // Obtém o dispositivo Mesh associado ao nó
        Ptr<MeshPointDevice> mpDevice = node->GetDevice(0)->GetObject<MeshPointDevice>();
        // Obtém o protocolo HWMP de um dos dispositivos mesh
        // Obtém o protocolo HWMP associado ao dispositivo Mesh
        Ptr<dot11s::HwmpProtocol> hwmp = mpDevice->GetObject<dot11s::HwmpProtocol>();
        if (hwmp)
        {
            std::cout << "\n=== Estatísticas do HWMP ===\n";
            // hwmp->Report(std::cout); // Imprime no terminal

            // Grava estatísticas num ficheiro
            std::ofstream outFile(
                "/home/mpais/ns-allinone-3.43/ns-3.43/scratch/meshtrace/hwmp_stats.txt");
            if (outFile.is_open())
            {
                hwmp->Report(outFile);
                outFile.close();
                std::cout << "Estatísticas do HWMP gravadas em hwmp_stats.txt" << std::endl;
            }
            else
            {
                std::cerr << "Erro ao abrir hwmp_stats.txt para escrita" << std::endl;
            }
        }
        else
        {
            std::cerr << "Erro: Protocolo HWMP não encontrado." << std::endl;
        } */
    // unsigned n(0);
    /* for (auto i = meshDevices.Begin(); i != meshDevices.End(); ++i, ++n)
    {
        std::ostringstream os;
        os << "mp-report-" << n << ".xml";
        std::cerr << "Printing mesh point device #" << n << " diagnostics to " << os.str() << "\n";
        std::ofstream of;
        of.open(os.str().c_str());
        if (!of.is_open())
        {
            std::cerr << "Error: Can't open file " << os.str() << "\n";
            return;
        }
        mesh.Report(*i, of);
        of.close();
    } */
}

int
main(int argc, char* argv[])
{
    // Enable packet metadata at the very start
    PacketMetadata::Enable();
    MeshTest t;
    t.Configure(argc, argv);
    return t.Run();
}
