#ifndef PHY_OCCUPANCY_H
#define PHY_OCCUPANCY_H

#include "ns3/core-module.h"
#include "ns3/mesh-point-device.h"
#include "ns3/network-module.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy-state.h"
#include "ns3/wifi-phy.h"

#include <iostream>
#include <limits>
#include <map>
#include <vector>

namespace ns3
{

// ---- per-node accumulators ----
struct PhyTimes
{
    double idle = 0.0, tx = 0.0, rx = 0.0, cca = 0.0;
};

std::map<uint32_t, PhyTimes>&
GetNodeTimes()
{
    static std::map<uint32_t, PhyTimes> m;
    return m;
}

// Keep sink objects alive for the whole sim
class PhyOccSink : public SimpleRefCount<PhyOccSink>
{
  public:
    explicit PhyOccSink(uint32_t nodeId)
        : m_nodeId(nodeId)
    {
    }

    // Signature for WifiPhy "State/State" traced callback (no context):
    //   void(Time start, Time duration, WifiPhyState state)
    void StateCb(Time start, Time duration, WifiPhyState state)
    {
        double d = duration.GetSeconds();
        auto& t = GetNodeTimes()[m_nodeId];
        switch (state)
        {
        case WifiPhyState::IDLE:
            t.idle += d;
            break;
        case WifiPhyState::TX:
            t.tx += d;
            break;
        case WifiPhyState::RX:
            t.rx += d;
            break;
        case WifiPhyState::CCA_BUSY:
            t.cca += d;
            break;
        default:
            break;
        }
    }

  private:
    uint32_t m_nodeId;
};

std::vector<Ptr<PhyOccSink>>&
GetSinks()
{
    static std::vector<Ptr<PhyOccSink>> v;
    return v;
}

// hook a single WifiNetDevice's PHY
void
HookOneWifi(Ptr<ns3::WifiNetDevice> wnd, uint32_t nodeId)
{
    if (!wnd)
        return;
    Ptr<ns3::WifiPhy> phy = wnd->GetPhy();
    if (!phy)
        return;

    // DEBUG: print types so we know what we’re attaching to
    std::cout << "[occ] node " << nodeId << " dev=" << wnd->GetInstanceTypeId().GetName()
              << " phy=" << phy->GetInstanceTypeId().GetName() << "\n";

    // Preferred: hook the State trace on the PHY's state helper
    Ptr<ns3::WifiPhyStateHelper> st = phy->GetState();
    if (st)
    {
        Ptr<PhyOccSink> sink = Create<PhyOccSink>(nodeId);
        bool ok =
            st->TraceConnectWithoutContext("State", ns3::MakeCallback(&PhyOccSink::StateCb, sink));
        std::cout << "[occ] " << (ok ? "hooked" : "missed")
                  << " State (via WifiPhyStateHelper) on node " << nodeId << "\n";
        if (ok)
            GetSinks().push_back(sink);
        return;
    }

    // Fallbacks (some builds still expose these on the PHY itself):
    {
        Ptr<PhyOccSink> sink = Create<PhyOccSink>(nodeId);
        bool ok =
            phy->TraceConnectWithoutContext("State", ns3::MakeCallback(&PhyOccSink::StateCb, sink));
        if (ok)
        {
            std::cout << "[occ] hooked Phy.State on node " << nodeId << "\n";
            GetSinks().push_back(sink);
            return;
        }
    }

    // As a last resort, you can infer busy time from TxBegin/TxEnd + RxBegin/RxEnd
    // (coarser, but at least gives activity). Only enable if you really need it.
    std::cout << "[occ] WARNING: no State trace found on node " << nodeId
              << " (will not compute %busy unless you enable a fallback)\n";
}

void
HookPhyOccupancyAllNodes()
{
    for (uint32_t i = 0; i < ns3::NodeList::GetNNodes(); ++i)
    {
        Ptr<ns3::Node> node = ns3::NodeList::GetNode(i);
        if (!node)
            continue;
        for (uint32_t j = 0; j < node->GetNDevices(); ++j)
        {
            Ptr<ns3::WifiNetDevice> wnd = node->GetDevice(j)->GetObject<ns3::WifiNetDevice>();
            if (wnd)
                HookOneWifi(wnd, node->GetId());
        }
    }
}

// hook all PHYs under a MeshPointDevice
void
HookMeshPoint(Ptr<MeshPointDevice> mpd, uint32_t nodeId)
{
    if (!mpd)
        return;
    for (uint32_t i = 0; i < mpd->GetNInterfaces(); ++i)
    {
        Ptr<NetDevice> iface = mpd->GetInterface(i);
        HookOneWifi(iface->GetObject<WifiNetDevice>(), nodeId);
    }
}

void
PrintPhyOccupancySummary()
{
    auto& M = GetNodeTimes();
    double aggIdle = 0, aggTx = 0, aggRx = 0, aggCca = 0;

    std::cout << "\n==== PHY channel occupancy (% busy) ====\n";
    for (const auto& kv : M)
    {
        uint32_t nodeId = kv.first;
        const auto& t = kv.second;
        double busy = t.tx + t.rx + t.cca;
        double total = busy + t.idle;
        if (total <= 0)
            continue;
        double occ = 100.0 * busy / total;

        std::cout << "Node " << nodeId << "  busy=" << occ << "%  "
                  << "(tx=" << t.tx << "s, rx=" << t.rx << "s, cca=" << t.cca
                  << "s, idle=" << t.idle << "s, total=" << total << "s)\n";

        aggIdle += t.idle;
        aggTx += t.tx;
        aggRx += t.rx;
        aggCca += t.cca;
    }
    double aggBusy = aggTx + aggRx + aggCca;
    double aggTotal = aggBusy + aggIdle;
    if (aggTotal > 0)
        std::cout << "Global occupancy: " << (100.0 * aggBusy / aggTotal) << "%\n";
    std::cout << "========================================\n";
}

void
PrintPhyOccupancySummary(std::ostream& os)
{
    auto& M = GetNodeTimes();
    double aggIdle = 0, aggTx = 0, aggRx = 0, aggCca = 0;

    os << "\n==== PHY channel occupancy (% busy) ====\n";
    for (const auto& kv : M)
    {
        uint32_t nodeId = kv.first;
        const auto& t = kv.second;
        double busy = t.tx + t.rx + t.cca;
        double total = busy + t.idle;
        if (total <= 0)
            continue;
        double occ = 100.0 * busy / total;

        os << "Node " << nodeId << "  busy=" << occ << "%  "
           << "(tx=" << t.tx << "s, rx=" << t.rx << "s, cca=" << t.cca
           << "s, idle=" << t.idle << "s, total=" << total << "s)\n";

        aggIdle += t.idle;
        aggTx += t.tx;
        aggRx += t.rx;
        aggCca += t.cca;
    }
    double aggBusy = aggTx + aggRx + aggCca;
    double aggTotal = aggBusy + aggIdle;
    if (aggTotal > 0)
        os << "Global occupancy: " << (100.0 * aggBusy / aggTotal) << "%\n";
    os << "========================================\n";
}

} // namespace ns3

#endif // PHY_OCCUPANCY_H
