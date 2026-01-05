#pragma once

#include "meter.h"

// Persistence
void storageLoad();
void storageSave();

// Access to persisted data (if needed)
PersistedData storageGetPersisted();
