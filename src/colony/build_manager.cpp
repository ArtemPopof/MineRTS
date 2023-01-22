#include "build_manager.h"
#include "events/colony_events.h"

void updateCurrentBuildingPlace(v3s16 position);

void BuildManager::startBuilding()
{
	currentState = CHOOSING_PLACE;
	client->getEventManager()->reg(MtEvent::POINTED_NODE_CHANGED, BuildManager::onPointedNodeChanged, this);
}

BuildManager::BuildManager(Client *client) : client(client)
{
    currentState = IDLE;
}

void BuildManager::onPointedNodeChanged(MtEvent *e, void *data)
{
	PointedNodeChangedEvent* event = (PointedNodeChangedEvent*) e;
	BuildManager* manager = (BuildManager*) data;
	if (manager->currentState != CHOOSING_PLACE) return;

    updateCurrentBuildingPlace(event->p.node_abovesurface);
}

void updateCurrentBuildingPlace(v3s16 position) {
	printf("newNodeposition: %d, %d, %d", position[0], position[1], position[2]);
}
