#include "MeshPacketQueue.h"
#include "configuration.h"
#include <assert.h>

#include <algorithm>

/// @return the priority of the specified packet
inline uint32_t getPriority(const meshtastic_MeshPacket *p)
{
    auto pri = p->priority;
    return pri;
}

/// @return "true" if "p1" is ordered before "p2"
bool CompareMeshPacketFunc(const meshtastic_MeshPacket *p1, const meshtastic_MeshPacket *p2)
{
    assert(p1 && p2);
    auto p1p = getPriority(p1), p2p = getPriority(p2);

    // If priorities differ, use that
    // for equal priorities, order by id (older packets have higher priority - this will briefly be wrong when IDs roll over but
    // no big deal)
    return (p1p != p2p)
               ? (p1p < p2p)                                                 // prefer bigger priorities
               : ((p1->id & ID_COUNTER_MASK) >= (p2->id & ID_COUNTER_MASK)); // Mask to counter portion, prefer smaller packet ids
}

MeshPacketQueue::MeshPacketQueue(size_t _maxLen) : maxLen(_maxLen) {}

bool MeshPacketQueue::empty()
{
    return queue.empty();
}

/**
 * Some clients might not properly set priority, therefore we fix it here.
 */
void fixPriority(meshtastic_MeshPacket *p)
{
    // We might receive acks from other nodes (and since generated remotely, they won't have priority assigned.  Check for that
    // and fix it
    if (p->priority == meshtastic_MeshPacket_Priority_UNSET) {
        // if a reliable message give a bit higher default priority
        p->priority = (p->want_ack ? meshtastic_MeshPacket_Priority_RELIABLE : meshtastic_MeshPacket_Priority_DEFAULT);
        if (p->which_payload_variant == meshtastic_MeshPacket_decoded_tag) {
            // if acks/naks give very high priority
            if (p->decoded.portnum == meshtastic_PortNum_ROUTING_APP) {
                p->priority = meshtastic_MeshPacket_Priority_ACK;
                // if text or admin, give high priority
            } else if (p->decoded.portnum == meshtastic_PortNum_TEXT_MESSAGE_APP ||
                       p->decoded.portnum == meshtastic_PortNum_ADMIN_APP) {
                p->priority = meshtastic_MeshPacket_Priority_HIGH;
                // if it is a response, give higher priority to let it arrive early and stop the request being relayed
            } else if (p->decoded.request_id != 0) {
                p->priority = meshtastic_MeshPacket_Priority_RESPONSE;
                // Also if we want a response, give a bit higher priority
            } else if (p->decoded.want_response) {
                p->priority = meshtastic_MeshPacket_Priority_RELIABLE;
            }
        }
    }
}

/** enqueue a packet, return false if full */
bool MeshPacketQueue::enqueue(meshtastic_MeshPacket *p)
{
    // no space - try to replace a lower priority packet in the queue
    if (queue.size() >= maxLen) {
        return replaceLowerPriorityPacket(p);
    }

    queue.push_back(p);
    std::push_heap(queue.begin(), queue.end(), &CompareMeshPacketFunc);
    return true;
}

meshtastic_MeshPacket *MeshPacketQueue::dequeue()
{
    if (empty()) {
        return NULL;
    }

    auto *p = queue.front();
    std::pop_heap(queue.begin(), queue.end(), &CompareMeshPacketFunc);
    queue.pop_back();

    return p;
}

meshtastic_MeshPacket *MeshPacketQueue::getFront()
{
    if (empty()) {
        return NULL;
    }

    auto *p = queue.front();
    return p;
}

/** Attempt to find and remove a packet from this queue.  Returns a pointer to the removed packet, or NULL if not found */
meshtastic_MeshPacket *MeshPacketQueue::remove(NodeNum from, PacketId id)
{
    for (auto it = queue.begin(); it != queue.end(); it++) {
        auto p = (*it);
        if (getFrom(p) == from && p->id == id) {
            queue.erase(it);
            std::make_heap(queue.begin(), queue.end(), &CompareMeshPacketFunc);
            return p;
        }
    }

    return NULL;
}

/** Attempt to find and remove a packet from this queue.  Returns the packet which was removed from the queue */
bool MeshPacketQueue::replaceLowerPriorityPacket(meshtastic_MeshPacket *p)
{
    std::sort_heap(queue.begin(), queue.end(), &CompareMeshPacketFunc); // sort ascending based on priority (0 -> 127)

    // find first packet which does not compare less (in priority) than parameter packet
    auto low = std::lower_bound(queue.begin(), queue.end(), p, &CompareMeshPacketFunc);

    if (low == queue.begin()) { // if already at start, there are no packets with lower priority
        return false;
    }

    if (low == queue.end()) {
        // all priorities in the vector are smaller than the incoming packet. Replace the lowest priority (first) element
        low = queue.begin();
    } else {
        // 'low' iterator points to first packet which does not compare less than parameter
        --low; // iterate to lower priority packet
    }

    if (getPriority(p) > getPriority(*low)) {
        packetPool.release(*low); // deallocate and drop the packet we're replacing
        *low = p;                 // replace low-pri packet at this position with incoming packet with higher priority
    }

    std::make_heap(queue.begin(), queue.end(), &CompareMeshPacketFunc);
    return true;
}