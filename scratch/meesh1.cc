#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/log.h"
#include "ns3/mesh-helper.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/onoff-application.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/wifi-module.h"

#include <sstream>
#include <vector>

using namespace ns3;

uint32_t packetsSent = 0;
std::vector<uint32_t> packetsReceived1;

// Callback function to count sent packets
void
PacketSentCallback(Ptr<const Packet> packet)
{
    packetsSent++;
}

// Callback function to count received packets using context
void
PacketReceivedCallback(std::string context, Ptr<const Packet> packet, const Address& address)
{
    uint32_t nodeIndex = 0;
    size_t startPos = context.find("/NodeList/");
    if (startPos != std::string::npos)
    {
        startPos += 10;
        size_t endPos = context.find('/', startPos);
        if (endPos != std::string::npos)
        {
            std::string nodeIdStr = context.substr(startPos, endPos - startPos);
            nodeIndex = std::stoul(nodeIdStr);
        }
    }

    packetsReceived1[nodeIndex]++;
}

NS_LOG_COMPONENT_DEFINE("MeshExample");

int
main(int argc, char* argv[])
{
    const std::string targetAddr = "239.192.100.1";
    Config::SetDefault("ns3::Ipv4L3Protocol::EnableDuplicatePacketDetection", BooleanValue(true));
    Config::SetDefault("ns3::Ipv4L3Protocol::DuplicateExpire", TimeValue(Seconds(10)));

    // Create nodes
    NodeContainer nodes;
    nodes.Create(5);

    // Name nodes
    Names::Add("A", nodes.Get(0));
    Names::Add("B", nodes.Get(1));
    Names::Add("C", nodes.Get(2));
    Names::Add("D", nodes.Get(3));
    Names::Add("E", nodes.Get(4));

    // Configure mesh network
    //WifiHelper wifi;
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

   /* wifiPhy.EnablePcapAll(std::string("mp")); */
  

    MeshHelper mesh = MeshHelper::Default();
    mesh.SetStackInstaller("ns3::Dot11sStack"); // Enable 802.11s mesh networking
    mesh.SetSpreadInterfaceChannels(MeshHelper::SPREAD_CHANNELS);
    mesh.SetMacType("RandomStart", TimeValue(Seconds(0.1)));
    mesh.SetNumberOfInterfaces(1);


    NetDeviceContainer meshDevices = mesh.Install(wifiPhy, nodes);
    wifiPhy.EnablePcapAll("/home/ricardosa/ns-allinone-3.43/ns-3.43/scratch/mesh1trace/mesh-hwmp");



    // Set up mobility (positions for nodes)
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX",
                                  DoubleValue(0.0),
                                  "MinY",
                                  DoubleValue(0.0),
                                  "DeltaX",
                                  DoubleValue(10.0),
                                  "DeltaY",
                                  DoubleValue(10.0),
                                  "GridWidth",
                                  UintegerValue(3),
                                  "LayoutType",
                                  StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);
    Ipv4AddressHelper ipv4address;
    ipv4address.SetBase("10.0.0.0", "255.255.255.0");
    ipv4address.Assign(meshDevices);

    // Set up sinks for the receivers
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), 9));
    auto sinks = sinkHelper.Install(nodes.Get(1)); // Node B
    sinks.Add(sinkHelper.Install(nodes.Get(2)));                   // Node C
    sinks.Add(sinkHelper.Install(nodes.Get(3)));                   // Node D
    sinks.Add(sinkHelper.Install(nodes.Get(4)));                   // Node E
    sinks.Start(Seconds(1.0));

    // Initialize the vector to hold packet counts for all nodes
    packetsReceived1.resize(nodes.GetN(), 0);

    // Connect received packet callback for each sink using context
    for (auto iter = sinks.Begin(); iter != sinks.End(); ++iter)
    {
        Ptr<PacketSink> sink = (*iter)->GetObject<PacketSink>();
        uint32_t nodeIndex = sink->GetNode()->GetId();

        std::ostringstream oss;
        oss << "/NodeList/" << nodeIndex << "/ApplicationList/*/$ns3::PacketSink/Rx";
        Config::Connect(oss.str(), MakeCallback(&PacketReceivedCallback));
    }

    // Set up the source to send multicast packets
    OnOffHelper onoffHelper("ns3::UdpSocketFactory",
                            InetSocketAddress(Ipv4Address(targetAddr.c_str()), 9));
    onoffHelper.SetAttribute("DataRate", DataRateValue(DataRate("8Mbps")));
    onoffHelper.SetAttribute("MaxBytes", UintegerValue(1024));
    auto source = onoffHelper.Install(nodes.Get(0)); // Node A
    source.Start(Seconds(1.1));
    source.Stop(Seconds(10.0));

    NS_LOG_INFO("Checking if OnOff Application is installed and started");
    // Connect sent packet callback
    source.Get(0)->GetObject<OnOffApplication>()->TraceConnectWithoutContext(
        "Tx",
        MakeCallback(PacketSentCallback));



        

    // Add animation
    // AnimationInterface anim("mesh-hwmp-animation.xml");
    std::cout << "Starting simulation..." << std::endl;

    Simulator::Stop(Seconds(100));
    // Run simulation
    Simulator::Run();
    std::cout << "Packets sent: " << packetsSent << std::endl;

    for (size_t i = 0; i < packetsReceived1.size(); ++i)
    {
        double deliveryRatio =
            (packetsSent > 0) ? static_cast<double>(packetsReceived1[i]) / packetsSent : 0.0;
        std::cout << "Node " << i << " received " << packetsReceived1[i]
                  << " packets. Delivery Ratio: " << deliveryRatio * 100 << "%" << std::endl;
    }

    Simulator::Destroy();
    Names::Clear();
    return 0;
}
