//Add commentMore actions
// Created by Eduardo Soares on 08/02/2023.
//

#include "lru-group-seqno.h"
#include "ns3/core-module.h"

using namespace ns3::dot11s;
NS_LOG_COMPONENT_DEFINE ("LruGroupSeqNo");
NS_OBJECT_ENSURE_REGISTERED(LruGroupSeqNo);
CacheItem::CacheItem (uint32_t _seqno)
{
  seqno = _seqno;
}
ns3::TypeId
LruGroupSeqNo::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::dot11s::LruGroupSeqNo")
                   .SetParent<Object> ()
                   .AddConstructor<LruGroupSeqNo> ()
                   .AddAttribute ("CacheSize", "Cache size of LRU size", UintegerValue (100),
                                  MakeUintegerAccessor (&LruGroupSeqNo::SetCacheSize,
                                                        &LruGroupSeqNo::GetCacheSize),
                                  MakeUintegerChecker<uint8_t> (0, 1024));
  return tid;
}

uint8_t
LruGroupSeqNo::GetCacheSize (void) const
{
  return cacheSize;
}
void
LruGroupSeqNo::SetCacheSize (uint8_t size)
{
  cacheSize = size;
  Condense ();
}

LruGroupSeqNo::LruGroupSeqNo ()
{
}

void
LruGroupSeqNo::Condense ()
{
  // while items list >= cache size, remove elements from list
  while (seqnos.size () > cacheSize)
    {
      seqnos.pop_back ();
    }
}

bool
LruGroupSeqNo::CheckSeen (uint32_t _seqno)
{
  NS_LOG_FUNCTION (this << _seqno);
  // check if list
  bool seen = false;
  // if seen, remove but keep info it was seen
  for (auto it = seqnos.begin (); it != seqnos.end (); it++)
    {
      if (it->seqno == _seqno)
        {
          seen = true;
          // remove it
          seqnos.erase (it);
          break;
        }
    }
  // add to list
  seqnos.insert (seqnos.begin (), CacheItem (_seqno));
  Condense ();
  return seen;
}