#pragma once

#include "client/client.h"
#include "client/event_manager.h"

enum BuildState {
    IDLE,
    CHOOSING_PLACE,
    BUILD
};

class BuildManager {
private:
    Client* client;
    BuildState currentState;
    
    static void onPointedNodeChanged(MtEvent *e, void *data);

public:
    BuildManager(Client* client);
    BuildManager() {};

    void startBuilding();
};
