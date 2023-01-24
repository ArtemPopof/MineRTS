#pragma once

#include "client/client.h"
#include "client/event_manager.h"
#include "client/clientmap.h"

#include "nodedef.h"
#include <vector3d.h>

enum BuildState {
    IDLE,
    CHOOSING_PLACE,
    BUILD
};

class Building {
private:
    std::map<irr::core::vector3d<s16>, content_t> *data;
    irr::core::vector3d<s16> position;
public:

    Building(std::map<irr::core::vector3d<s16>, content_t> *data) : data(data) 
    {
        position = irr::core::vector3d<s16>(0, 0, 0);
    };

    ~Building() 
    {
        delete data;
    }

    Building *translate(irr::core::vector3d<s16> offset);
    Building *setOffset(irr::core::vector3d<s16> offset);
    std::map<irr::core::vector3d<s16>, content_t> *getData() { return data; };
};

class PlannedBuilding {
private:
    Building *building;
    Building *substitutedNodes;
    Client *client;

public:
    PlannedBuilding(Building *building, irr::core::vector3d<s16> position, Client *client);
    ~PlannedBuilding()
    {
        delete building;
        delete substitutedNodes;
    }
    void setPosition(irr::core::vector3d<s16> position);
};

class BuildManager {
private:
    Client *client;
    BuildState currentState;

    PlannedBuilding *plannedBuilding;
    
    static void onPointedNodeChanged(MtEvent *e, void *data);

public:
    BuildManager(Client *client);
    BuildManager() {};

    void startBuilding();
};
