#include "build_manager.h"
#include "events/colony_events.h"
#include "mapgen/mapgen_v6.h"
#include "client/clientmap.h"

void updateCurrentBuildingPlace(PlannedBuilding *plannedBuilding, Client *client);
PlannedBuilding* createPlannedBuilding(v3s16 position, Client* client);

void BuildManager::startBuilding()
{
	currentState = CHOOSING_PLACE;
	client->getEventManager()->reg(MtEvent::POINTED_NODE_CHANGED, BuildManager::onPointedNodeChanged, this);
}

BuildManager::BuildManager(Client *client) : client(client), plannedBuilding(nullptr)
{
    currentState = IDLE;
}

void BuildManager::onPointedNodeChanged(MtEvent *e, void *data)
{
	PointedNodeChangedEvent* event = (PointedNodeChangedEvent*) e;
	BuildManager* manager = (BuildManager*) data;
	if (manager->currentState != CHOOSING_PLACE) return;

	if (manager->plannedBuilding == nullptr) {
		auto plannedBuilding = createPlannedBuilding(event->p.node_abovesurface, manager->client);
		manager->plannedBuilding = plannedBuilding;
	} else {
    	updateCurrentBuildingPlace(manager->plannedBuilding, manager->client);
	}
}

PlannedBuilding* createPlannedBuilding(v3s16 position, Client* client) {
	auto buildingData = new std::map<v3s16, content_t>();
	(*buildingData)[position + v3s16(0, 0, 0)] = client->getNodeDefManager()->getId("mapgen_stone");
	(*buildingData)[position + v3s16(0, 1, 0)] = client->getNodeDefManager()->getId("mapgen_stone");
	(*buildingData)[position + v3s16(0, 2, 0)] = client->getNodeDefManager()->getId("mapgen_dirt");
	(*buildingData)[position + v3s16(0, 3, 0)] = client->getNodeDefManager()->getId("mapgen_dirt");
	auto building = new Building(buildingData);
	auto plannedBuilding = new PlannedBuilding(building, position, client);
	
	for (auto iter = buildingData->begin(); iter != buildingData->end(); iter++) {
		client->addNode(iter->first, iter->second);
	}

	printf("created planned building");

	return plannedBuilding;
}

void updateCurrentBuildingPlace(PlannedBuilding *plannedBuilding, Client *client) {
	printf("update planned building");
}

Building *Building::translate(irr::core::vector3d<s16> offset)
{
	this->position += offset;
	
	return this;
}

Building *Building::setOffset(irr::core::vector3d<s16> offset)
{
	this->position = offset;
	return this;
}

PlannedBuilding::PlannedBuilding(Building *building, irr::core::vector3d<s16> position, Client* client) 
: building(building), client(client)
{
	auto substitutedNodesData = new std::map<irr::core::vector3d<s16>, content_t>();

	auto buildingData = building->getData();
	for (auto iter = buildingData->begin(); iter != buildingData->end(); iter++) {
		auto nodePos = iter->first;
		auto nodeContent = client->getEnv().getMap().getNode(nodePos).getContent();

		(*substitutedNodesData)[nodePos] = nodeContent;
	}

	substitutedNodes = new Building(substitutedNodesData);

	building->setOffset(position);
}


void PlannedBuilding::setPosition(irr::core::vector3d<s16> position)
{
}
