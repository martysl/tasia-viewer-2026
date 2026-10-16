// Copyright (c) 2026 Tasia. SPDX-License-Identifier: LGPL-2.1-only
#include "llviewerprecompiledheaders.h"
#include "llfloatergridmanager.h"

#include "llbutton.h"
#include "lllineeditor.h"
#include "llpanellogin.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "llviewernetwork.h"

LLFloaterGridManager::LLFloaterGridManager(const LLSD& key) : LLFloater(key)
{
}

void LLFloaterGridManager::onAddGrid()
{
    addGrid();
}

void LLFloaterGridManager::onRemoveGrid()
{
    removeGrid();
}

void LLFloaterGridManager::onClose()
{
    closeFloater();
}

void LLFloaterGridManager::onListSelection()
{
    updateSelection();
}

bool LLFloaterGridManager::postBuild()
{
    getChild<LLButton>("add_grid")->setCommitCallback(boost::bind(&LLFloaterGridManager::onAddGrid, this));
    getChild<LLButton>("remove_grid")->setCommitCallback(boost::bind(&LLFloaterGridManager::onRemoveGrid, this));
    getChild<LLButton>("close")->setCommitCallback(boost::bind(&LLFloaterGridManager::onClose, this));
    getChild<LLScrollListCtrl>("grid_list")->setCommitCallback(boost::bind(&LLFloaterGridManager::onListSelection, this));
    getChild<LLScrollListCtrl>("grid_list")->setCommitOnSelectionChange(true);
    refresh();
    return true;
}

void LLFloaterGridManager::onOpen(const LLSD&)
{
    getChild<LLTextBox>("status")->setValue(getString("choose_grid"));
    refresh();
}

void LLFloaterGridManager::refresh()
{
    LLScrollListCtrl* list = getChild<LLScrollListCtrl>("grid_list");
    const LLSD selected = list->getSelectedValue();
    list->deleteAllItems();
    LLGridManager* manager = LLGridManager::getInstance();
    for (const auto& grid : manager->getKnownGrids())
    {
        std::vector<std::string> uris;
        manager->getLoginURIs(grid.first, uris);
        LLSD row;
        row["id"] = grid.first;
        row["columns"][0]["column"] = "label";
        row["columns"][0]["value"] = grid.second;
        row["columns"][1]["column"] = "login_uri";
        row["columns"][1]["value"] = uris.empty() ? "" : uris.front();
        list->addElement(row);
    }
    list->sortByColumnIndex(0, true);
    list->setSelectedByValue(selected, true);
    updateSelection();
}

void LLFloaterGridManager::updateSelection()
{
    LLScrollListCtrl* list = getChild<LLScrollListCtrl>("grid_list");
    const std::string selected = list->getSelectedValue().asString();
    LLGridManager* manager = LLGridManager::getInstance();
    getChild<LLButton>("remove_grid")->setEnabled(list->getFirstSelected() &&
        !manager->isSystemGrid(selected) && selected != manager->getGrid());
}

void LLFloaterGridManager::addGrid()
{
    std::string error;
    const bool added = LLGridManager::getInstance()->addCustomGrid(
        getChild<LLLineEditor>("login_uri")->getText(),
        getChild<LLLineEditor>("grid_label")->getText(), error);
    getChild<LLTextBox>("status")->setValue(added ? getString("added_grid") : error);
    if (added)
    {
        getChild<LLLineEditor>("login_uri")->clear();
        getChild<LLLineEditor>("grid_label")->clear();
        refresh();
        LLPanelLogin::refreshGridList();
    }
}

void LLFloaterGridManager::removeGrid()
{
    std::string error;
    const bool removed = LLGridManager::getInstance()->removeCustomGrid(
        getChild<LLScrollListCtrl>("grid_list")->getSelectedValue().asString(), error);
    getChild<LLTextBox>("status")->setValue(removed ? getString("removed_grid") : error);
    if (removed)
    {
        refresh();
        LLPanelLogin::refreshGridList();
    }
}
