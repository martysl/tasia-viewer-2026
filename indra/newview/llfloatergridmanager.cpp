// Copyright (c) 2026 Tasia. SPDX-License-Identifier: LGPL-2.1-only
#include "llviewerprecompiledheaders.h"
#include "llfloatergridmanager.h"

#include "llbutton.h"
#include "lllineeditor.h"
#include "llpanellogin.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "llviewernetwork.h"
#include "lltasia_config_fetch.h"
#include "lluri.h"
#include "llxmlnode.h"
#include "llsdutil.h"
#include "llviewercontrol.h"

namespace
{
const char* metadataKeys[] = {"login_page", "helper_uri", "web_profile_url", "register",
    "password", "help", "about", "search", "gatekeeper"};
}

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
    ++mDiscoveryGeneration;
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
    getChild<LLButton>("discover_grid")->setCommitCallback([this](LLUICtrl*, const LLSD&) { discoverGrid(); });
    getChild<LLButton>("save_metadata")->setCommitCallback([this](LLUICtrl*, const LLSD&) { saveMetadata(); });
    getChild<LLButton>("update_catalog")->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        const auto weak = getHandle();
        LLGridManager::getInstance()->requestGridCatalog([weak](bool ok)
        {
            auto* self = dynamic_cast<LLFloaterGridManager*>(weak.get());
            if (!self) return;
            self->getChild<LLTextBox>("status")->setValue(self->getString(ok ? "catalog_updated" : "catalog_failed"));
            if (ok) self->refresh();
        });
    });
    getChild<LLScrollListCtrl>("grid_list")->setCommitCallback(boost::bind(&LLFloaterGridManager::onListSelection, this));
    getChild<LLScrollListCtrl>("grid_list")->setCommitOnSelectionChange(true);
    refresh();
    return true;
}

void LLFloaterGridManager::onOpen(const LLSD&)
{
    getChild<LLTextBox>("status")->setValue(getString("choose_grid"));
    refresh();
    LLGridManager::getInstance()->requestGridCatalog();
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
    getChild<LLButton>("save_metadata")->setEnabled(list->getFirstSelected() && !manager->isSystemGrid(selected));
    ++mDiscoveryGeneration;
    if (list->getFirstSelected())
    {
        const LLSD data = manager->getGridInfo(selected);
        showMetadata(data);
        getChild<LLLineEditor>("grid_label")->setText(data["label"].asString());
        if (data["login_uri"].isArray() && data["login_uri"].size())
            getChild<LLLineEditor>("login_uri")->setText(data["login_uri"][0].asString());
    }
}

void LLFloaterGridManager::addGrid()
{
    std::string error;
    const bool added = LLGridManager::getInstance()->addCustomGrid(
        getChild<LLLineEditor>("login_uri")->getText(),
        getChild<LLLineEditor>("grid_label")->getText(), error, metadata());
    getChild<LLTextBox>("status")->setValue(added ? getString("added_grid") : error);
    if (added)
    {
        getChild<LLLineEditor>("login_uri")->clear();
        getChild<LLLineEditor>("grid_label")->clear();
        refresh();
        LLPanelLogin::refreshGridList();
    }
}

LLSD LLFloaterGridManager::metadata() const
{
    LLSD data = LLSD::emptyMap();
    for (const auto* key : metadataKeys) data[key] = getChild<LLLineEditor>(key)->getText();
    data["label"] = getChild<LLLineEditor>("grid_label")->getText();
    return data;
}

void LLFloaterGridManager::showMetadata(const LLSD& data)
{
    for (const auto* key : metadataKeys) getChild<LLLineEditor>(key)->setText(data[key].asString());
}

void LLFloaterGridManager::saveMetadata()
{
    ++mDiscoveryGeneration;
    std::string error;
    const auto selected = getChild<LLScrollListCtrl>("grid_list")->getSelectedValue().asString();
    const auto existing = LLGridManager::getInstance()->getGridInfo(selected)["login_uri"];
    if (existing.isArray() && existing.size() &&
        existing[0].asString() != getChild<LLLineEditor>("login_uri")->getText())
    {
        getChild<LLTextBox>("status")->setValue(getString("new_endpoint"));
        return;
    }
    if (LLGridManager::getInstance()->setGridMetadata(selected, metadata(), error))
    {
        getChild<LLTextBox>("status")->setValue(getString("saved_metadata"));
        refresh();
        LLPanelLogin::refreshGridList();
    }
    else getChild<LLTextBox>("status")->setValue(error);
}

void LLFloaterGridManager::discoverGrid()
{
    const std::string login = getChild<LLLineEditor>("login_uri")->getText();
    const LLURI uri(login);
    if (uri.scheme() != "http" && uri.scheme() != "https")
    { getChild<LLTextBox>("status")->setValue(getString("discovery_failed")); return; }
    const auto generation = ++mDiscoveryGeneration;
    const auto weak = getHandle();
    const auto before = metadata();
    getChild<LLTextBox>("status")->setValue(getString("discovering"));
    tasiaFetchConfig(uri.scheme() + "://" + uri.authority() + "/get_grid_info", 65536,
        [weak, generation, login, before](bool ok, const std::string& text)
        {
            auto* self = dynamic_cast<LLFloaterGridManager*>(weak.get());
            if (!self || self->mDiscoveryGeneration != generation ||
                self->getChild<LLLineEditor>("login_uri")->getText() != login || !llsd_equals(self->metadata(), before)) return;
            LLXMLNodePtr root;
            const auto lower = utf8str_tolower(text);
            if (!ok || text.find('\0') != std::string::npos || lower.find("<!doctype") != std::string::npos || lower.find("<!entity") != std::string::npos ||
                !LLXMLNode::parseBuffer(text.data(), text.size(), root) || root.isNull() || !root->hasName("gridinfo") ||
                root->getChildCount() > 64)
            { self->getChild<LLTextBox>("status")->setValue(self->getString("discovery_failed")); return; }
            LLSD data = before;
            const std::map<std::string, std::string> names = {{"welcome", "login_page"},
                {"economy", "helper_uri"}, {"helperuri", "helper_uri"}, {"profileuri", "web_profile_url"},
                {"web_profile_url", "web_profile_url"}, {"register", "register"}, {"password", "password"},
                {"help", "help"}, {"about", "about"}, {"search", "search"}, {"gatekeeper", "gatekeeper"}};
            for (auto node = root->getFirstChild(); node.notNull(); node = node->getNextSibling())
            {
                const std::string name = node->getName()->mString;
                const auto found = names.find(name);
                if (found != names.end() && node->getValue().size() <= 2048)
                    data[found->second] = node->getValue();
                if (name == "gridname" && node->getValue().size() <= 128)
                    data["label"] = node->getValue();
            }
            if (!data["gatekeeper"].asString().empty() && data["gatekeeper"].asString().find("://") == std::string::npos)
                data["gatekeeper"] = LLURI(login).scheme() + "://" + data["gatekeeper"].asString();
            self->showMetadata(data);
            self->getChild<LLLineEditor>("grid_label")->setText(data["label"].asString());
            self->getChild<LLTextBox>("status")->setValue(self->getString("discovered"));
        }, true);
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
