#include "build_manager.h"
#include "events/colony_events.h"
#include "mapgen/mapgen_v6.h"
#include "client/clientmap.h"

void updateCurrentBuildingPlace(PlannedBuilding *plannedBuilding, v3s16 position);
PlannedBuilding* createPlannedBuilding(v3s16 position, Client* client);
std::map<v3s16, content_t> *updateSubstitutedNodes(PlannedBuilding *building, Client *client, v3s16 newPosition);

void BuildManager::startBuilding()
{
	currentState = CHOOSING_PLACE;
	client->getEventManager()->reg(MtEvent::POINTED_NODE_CHANGED, BuildManager::onPointedNodeChanged, this);
}

void BuildManager::commitBuildingProject()
{
	currentState = IDLE;
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
    	updateCurrentBuildingPlace(manager->plannedBuilding, event->p.node_abovesurface);
	}
}

PlannedBuilding* createPlannedBuilding(v3s16 position, Client* client) 
{
	auto buildingData = new std::map<v3s16, content_t>();
	(*buildingData)[v3s16(0, 0, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(1, 0, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(2, 0, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(3, 0, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");

	(*buildingData)[v3s16(3, 0, 1)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(3, 0, 2)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(3, 0, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");

	(*buildingData)[v3s16(2, 0, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(1, 0, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(0, 0, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");

	(*buildingData)[v3s16(0, 0, 2)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(0, 0, 1)] = client->getNodeDefManager()->getId("mapgen_cobble");


	(*buildingData)[v3s16(0, 1, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(1, 1, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(2, 1, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(3, 1, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");

	(*buildingData)[v3s16(3, 1, 1)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(3, 1, 2)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(3, 1, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");

	(*buildingData)[v3s16(2, 1, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(1, 1, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(0, 1, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");

	(*buildingData)[v3s16(0, 1, 2)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(0, 1, 1)] = client->getNodeDefManager()->getId("mapgen_cobble");

	(*buildingData)[v3s16(0, 2, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(1, 2, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(2, 2, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(3, 2, 0)] = client->getNodeDefManager()->getId("mapgen_cobble");

	(*buildingData)[v3s16(3, 2, 1)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(3, 2, 2)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(3, 2, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");

	(*buildingData)[v3s16(2, 2, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(1, 2, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(0, 2, 3)] = client->getNodeDefManager()->getId("mapgen_cobble");

	(*buildingData)[v3s16(0, 2, 2)] = client->getNodeDefManager()->getId("mapgen_cobble");
	(*buildingData)[v3s16(0, 2, 1)] = client->getNodeDefManager()->getId("mapgen_cobble");

	auto building = new Building(buildingData);
	auto plannedBuilding = new PlannedBuilding(building, position, client);
	
	building->addBuildingToWorld(client);

	printf("created planned building");

	return plannedBuilding;
}

std::map<v3s16, content_t> *updateSubstitutedNodes(PlannedBuilding *building, Client *client, v3s16 newPosition)
{
	auto nodePos = building->getBuilding()->getData();
	auto substitutedData = building->getSubstitutedNodes()->getData();
	for (auto iter = nodePos->begin(); iter != nodePos->end(); iter++) {
		auto nodePos = iter->first;
		auto nodeContent = client->getEnv().getMap().getNode(nodePos + newPosition).getContent();

		(*substitutedData)[nodePos] = nodeContent;
	}

	building->getSubstitutedNodes()->setOffset(newPosition);

	return substitutedData;
}

void updateCurrentBuildingPlace(PlannedBuilding *plannedBuilding, v3s16 position) 
{
	plannedBuilding->setPosition(position);
}

void Building::addBuildingToWorld(Client *client)
{
	for (auto iter = data->begin(); iter != data->end(); iter++) {
		MapNode node = iter->second;
		node.setParam2(PROTOTYPE_NODE);
		client->addNode(iter->first + position, node);
	}
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

void Building::print()
{
	printf("Building %f:\n ", this);
	printf("pos: (%d, %d, %d)\n", position.X, position.Y, position.Z);

	for (auto iter = data->begin(); iter != data->end(); iter++) {
		printf("[%d, %d, %d] = %d\n", iter->first.X, iter->first.Y, iter->first.Z, iter->second);
	}
}

PlannedBuilding::PlannedBuilding(Building *building, irr::core::vector3d<s16> position, Client* client) 
: building(building), client(client)
{
	auto substitutedNodesData = new std::map<irr::core::vector3d<s16>, content_t>();
	substitutedNodes = new Building(substitutedNodesData);

	substitutedNodesData = updateSubstitutedNodes(this, client, position);

	building->setOffset(position);
}


void PlannedBuilding::setPosition(irr::core::vector3d<s16> position)
{
	building->print();
	printf("substituted ");
	substitutedNodes->print();
	auto oldPosition = this->building->getPosition();
	auto oldSubstitutedNodes = new std::map<v3s16, content_t>(*substitutedNodes->getData());

	// replace building nodes at old position with oldSubstitutedNodes
	for (auto iter = oldSubstitutedNodes->begin(); iter != oldSubstitutedNodes->end(); iter++) {
		client->addNode(iter->first + oldPosition, iter->second);
	}

	// update new substituted nodes
	updateSubstitutedNodes(this, client, position);

	// add new building nodes
	building->setOffset(position);
	building->addBuildingToWorld(client);
}
