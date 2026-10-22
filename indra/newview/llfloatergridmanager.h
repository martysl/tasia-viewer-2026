// Copyright (c) 2026 Tasia. SPDX-License-Identifier: LGPL-2.1-only
#ifndef LL_LLFLOATERGRIDMANAGER_H
#define LL_LLFLOATERGRIDMANAGER_H

#include "llfloater.h"

class LLFloaterGridManager : public LLFloater
{
public:
    explicit LLFloaterGridManager(const LLSD& key);
    bool postBuild() override;
    void onOpen(const LLSD& key) override;

private:
    void refresh();
    void updateSelection();
    void addGrid();
    void removeGrid();
    void onAddGrid();
    void onRemoveGrid();
    void onClose();
    void onListSelection();
    void discoverGrid();
    void saveMetadata();
    LLSD metadata() const;
    void showMetadata(const LLSD& data);
    U32 mDiscoveryGeneration = 0;
};

#endif
