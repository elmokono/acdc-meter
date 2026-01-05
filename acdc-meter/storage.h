#pragma once

#include "meter.h"

// Persistencia
void storageLoad();
void storageSave();

// Acceso a los datos persistidos (si se necesita)
PersistedData storageGetPersisted();
