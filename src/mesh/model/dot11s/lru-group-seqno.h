//
// Created by Eduardo Soares on 08/02/2023.
//

#ifndef NS3_LRU_GROUP_SEQNO_H
#define NS3_LRU_GROUP_SEQNO_H
#include "ns3/object.h"
namespace ns3 {
namespace dot11s {

class CacheItem
{
public:
  uint32_t seqno;
  CacheItem (uint32_t seqno);
};

class LruGroupSeqNo : public ns3::Object
{

public:
  LruGroupSeqNo ();
  bool CheckSeen (uint32_t seqno);
  static ns3::TypeId GetTypeId (void);

private:
  std::list<CacheItem> seqnos;
  uint8_t cacheSize;
  void Condense ();
  void SetCacheSize (uint8_t size);
  uint8_t GetCacheSize (void) const;
};

} // namespace dot11s
} // namespace ns3

#endif //NS3_LRU_GROUP_SEQNO_H