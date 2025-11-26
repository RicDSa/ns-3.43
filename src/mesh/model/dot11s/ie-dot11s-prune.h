#ifndef IE_DOT11S_PRUNE_H
#define IE_DOT11S_PRUNE_H

#include "ns3/mac48-address.h"
#include "ns3/wifi-information-element.h"

#include <vector>

namespace ns3
{
namespace dot11s
{

/**
 * \ingroup dot11s
 * \brief PRUNE information element for mesh path management
 *
 * This class represents a custom PRUNE IE used to inform nodes to stop forwarding
 * packets to certain destinations. This implementation is compatible with the
 * NS-3 WifiInformationElement framework.
 */
class IePrune : public WifiInformationElement
{
  public:
    IePrune();
    ~IePrune() override;
    /**
     * \brief Add a (destination, reasonCode) unit to the PRUNE message
     * \param destination MAC address to prune
     * \param reasonCode A 32-bit reason code
     */
    void AddPruneUnit(Mac48Address destination, uint32_t reasonCode);
    /**
     * \brief Set all prune entries at once
     */
    void SetEntries(const std::vector<std::pair<Mac48Address, uint32_t>>& entries);

    /**
     * \brief Get all destination/reason pairs contained in this PRUNE element
     * \returns vector of (Mac48Address, reasonCode)
     */
    std::vector<std::pair<Mac48Address, uint32_t>> GetPruneUnits() const;
    /**
     * \brief Set the intended receiver MAC address
     */
    void SetReceiver(Mac48Address receiver);
    Mac48Address GetReceiver() const;

    /**
     * \brief Set the interface index this PRUNE is sent from
     */
    void SetInterface(uint32_t interface);
    uint32_t GetInterface() const;

    /**
     * \brief Set the TTL for this PRUNE message
     */
    void SetTtl(uint8_t ttl);
    uint8_t GetTtl() const;

    /**
     * \brief Set the group address for multicast pruning
     * \param group The group address to set
     *
     * This is used to specify the multicast group for which the PRUNE applies.
     */
    void SetGroup(Mac48Address group);
    Mac48Address GetGroup() const;

    /**
     * \brief Set the originator of the Packet
     * \param originator The MAC address of the originator
     *
     * This is used to identify which node initiated the Packet
     */
    void SetOriginator(Mac48Address originator);
    Mac48Address GetOriginator() const;

    // Inherited from WifiInformationElement
    WifiInformationElementId ElementId() const override;
    void SerializeInformationField(Buffer::Iterator i) const override;
    uint16_t DeserializeInformationField(Buffer::Iterator start, uint16_t length) override;
    uint16_t GetInformationFieldSize() const override;
    void Print(std::ostream& os) const override;

  private:
    // List of pruned destinations and their reason codes
    std::vector<std::pair<Mac48Address, uint32_t>> m_pruneUnits;
    Mac48Address m_receiver; //!< Receiver of the PRUNE message
    uint32_t m_interface;    //!< Interface index used to send PRUNE
    uint8_t m_ttl;           //!< TTL value of PRUNE message
    Mac48Address m_group;    //!< Group address for multicast pruning
    Mac48Address m_originator; //!< Originator of the Packet
};
} // namespace dot11s
} // namespace ns3

#endif // IE_DOT11S_PRUNE_H