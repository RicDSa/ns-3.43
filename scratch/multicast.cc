#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-module.h"

using namespace ns3;

uint32_t g_TxCount = 0; //!< Tx packet counter.
uint32_t g_RxCount = 0; //!< Rx packet counter.
NS_LOG_COMPONENT_DEFINE("UdpSocketFactory");

// std::vector<uint32_t> packetsReceivedPerNode(6, 0); // Track packets received by each node

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
    Simulator::Schedule(Seconds(1.0),
                        &SendMulticastPacket,
                        socket,
                        packetSize,
                        multicastGroup,
                        remote);
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
    } 

    // Recebe o pacote
    while ((packet = socket->RecvFrom(from)))
    {
        // Verifica se o endereço de origem é IPv4
        if (InetSocketAddress::IsMatchingType(from))
        {
            // Converte o endereço de origem para InetSocketAddress (IPv4)
            InetSocketAddress addr = InetSocketAddress::ConvertFrom(from);

            // packetsReceivedPerNode[nodeId]++; // Increment received packets for this node

            // Imprime o endereço IPv4 de origem e a porta
            std::cout << "Pacote recebido de " << addr.GetIpv4() << " na porta " << addr.GetPort()
                      << std::endl;
        }
    }
    g_RxCount++;
}

int
main(int argc, char* argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer meshNodes;
    meshNodes.Create(3); // Create 6 mesh nodes

    // Create Wi-Fi PHY and channel helpers
    WifiHelper wifi;
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();

    wifiPhy.SetChannel(wifiChannel.Create());

    // Configure mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(50.0),
                                  "DeltaY",
                                  DoubleValue(50.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(meshNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(meshNodes);

    // Configure and install mesh
    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack");
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);
    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, meshNodes);

    wifiPhy.EnablePcapAll("mesh-hwmp");

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(meshDevices);

    //  Create a multicast group
    Ipv4Address multicastGroup = Ipv4Address("225.1.2.5"); // Multicast group address
    uint16_t multicastPort = 8080;                         // Multicast port

    // Configure multicast routing

    /* std::vector<uint32_t> outputInterfaces;
    outputInterfaces.push_back(1); */ // Assuming interface 1 for all nodes
    Ipv4StaticRoutingHelper multicastRoutingHelper;
    Ptr<Ipv4StaticRouting> multicastRouting;

    for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
    {
        Ptr<Node> node = meshNodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        ipv4->SetAttribute("IpForward", BooleanValue(true));
    }

    for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
    {
        Ptr<Node> node = meshNodes.Get(i);
        uint32_t inputInterface =
            node->GetObject<Ipv4>()->GetInterfaceForAddress(interfaces.GetAddress(i));
        std::vector<unsigned int> outputInterfaces;
        outputInterfaces.push_back(inputInterface);

        Ptr<Ipv4StaticRouting> staticRouting =
            multicastRoutingHelper.GetStaticRouting(node->GetObject<Ipv4>());

        // Add a multicast route for the multicast group address
        staticRouting->AddMulticastRoute(Ipv4Address::GetAny(), // Source address
                                         multicastGroup,        // Multicast group address
                                         1,                     // Input interface (1 = meshDevices)
                                         outputInterfaces       // Output interfaces (to all)
        );

        Ptr<Node> sender = meshNodes.Get(0);
        Ptr<NetDevice> senderIf = meshDevices.Get(0);
        multicastRoutingHelper.SetDefaultMulticastRoute(sender, senderIf);

    }


    // Print node IP addresses
    /* for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
    {
        Ptr<Node> node = meshNodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();

        // Check each interface on the node and print its index and address
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j)
        {
            for (uint32_t k = 0; k < ipv4->GetNAddresses(j); ++k)
            {
                Ipv4Address addr = ipv4->GetAddress(j, k).GetLocal();
                std::cout << "Node " << i << " Interface " << j << " IP Address: " << addr
                          << std::endl;
            }
        }
    }
 */

    // Set up a multicast receiver on the node
    for (uint32_t i = 0; i < meshNodes.GetN(); ++i)
    {
        Ptr<Socket> recvSink =
            Socket::CreateSocket(meshNodes.Get(i), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), multicastPort); //
        // Define o endereço IP e a porta onde o socket irá "escutar"

        recvSink->SetAllowBroadcast(true); // Permite que o socket receba pacotes broadcast
        recvSink->SetRecvCallback(MakeCallback(&ReceivePacket)); // Define a função de callback que será chamada toda vez que o socket receber um pacote

        int32_t bindResult =
            recvSink->Bind(local); // Faz a ligação do socket ao endereço e porta configurados.
        if (bindResult == 0)
        {
            // Bind was successful
            std::cout << "Socket successfully bound to " << multicastGroup << ":" << multicastPort
                      << std::endl;
        }
        else
        {
            std::cout << "Socket bind failed with error code: " << bindResult << std::endl;
            // Bind failed
        }
    }

    // Set up a multicast sender on the first node
    // Obtém o objeto Ipv4 do nó 0
    Ptr<Ipv4> ipv4 = meshNodes.Get(0)->GetObject<Ipv4>();

    // Obtém o endereço IP da interface 1 do nó 0
    Ipv4Address ipAddress = ipv4->GetAddress(1, 0).GetLocal();

    Ptr<Socket> source = Socket::CreateSocket(meshNodes.Get(0), UdpSocketFactory::GetTypeId());
    source->Bind(InetSocketAddress(
        ipAddress,
        8080)); // Liga o socket ao endereço IP "10.1.1.1" e à porta 8080 no emissor.
    InetSocketAddress remote = InetSocketAddress(
        multicastGroup,
        multicastPort); // Define o endereço de destino do multicast (grupo multicast e porta)
    source->SetAllowBroadcast(true); // Permite que o socket envie pacotes em broadcast

    int connectionResult =
        source->Connect(remote); // Estabelece a conexão com o grupo multicast e a porta
    if (connectionResult == 0)
    {
        std::cout << "Conexão com o endereço multicast " << multicastGroup << " na porta "
                  << multicastPort << " estabelecida com sucesso." << std::endl;
    }
    else
    {
        std::cout << "Falha ao conectar ao endereço multicast." << std::endl;
    }

    // Schedule the periodic sending of multicast packets
    uint32_t packetSize = 1024;
    Simulator::ScheduleWithContext(source->GetNode()->GetId(),
                                   Seconds(1.0),
                                   &SendMulticastPacket,
                                   source,
                                   packetSize,
                                   multicastGroup,
                                   remote);

    // Run the simulation
    Simulator::Stop(Seconds(5.0));
    Simulator::Run();

    std::cout << "Packets sent: " << g_TxCount << " received: " << g_RxCount
              << std::endl; // Print total packets sent and received

    // Calculate delivery ratio
    double deliveryRatio =
        (g_TxCount > 0) ? (static_cast<double>(g_RxCount) / g_TxCount) * 100.0 : 0.0;

    std::cout << "Delivery Ratio: " << deliveryRatio << "%" << std::endl;

    /*     for (uint32_t i = 0; i < packetsReceivedPerNode.size(); ++i)
        {
            double deliveryRatio = (g_TxCount > 0) ? (static_cast<double>(packetsReceivedPerNode[i])
       / g_TxCount) * 100.0 : 0.0; std::cout << "Node " << i << " - Packets Received: " <<
       packetsReceivedPerNode[i] << ", Delivery Ratio: " << deliveryRatio << "%" << std::endl;
        }
     */
    Simulator::Destroy();

    return 0;
}