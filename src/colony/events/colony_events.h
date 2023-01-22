#pragma once

#include "util/pointedthing.h"
#include "mapnode.h"

class PointedNodeChangedEvent : public MtEvent
{
public:
	MapNode n;
	PointedThing &p;


	PointedNodeChangedEvent(PointedThing& pointedThing, MapNode n):
		n(n),
		p(pointedThing)
	{}
	Type getType() const { return POINTED_NODE_CHANGED; }
};