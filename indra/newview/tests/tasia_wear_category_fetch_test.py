#!/usr/bin/env python3
"""Verify incomplete category fetches cannot proceed to outfit mutation."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = (ROOT / 'indra/newview/llappearancemgr.cpp').read_text()
BACKGROUND_FETCH = (ROOT / 'indra/newview/llinventorymodelbackgroundfetch.cpp').read_text()
AIS = (ROOT / 'indra/newview/llaisapi.cpp').read_text()


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


finalize = function(SOURCE, 'void LLAppearanceMgr::wearCategoryFinal(')
guard = finalize.index('if (!cat || !gInventory.isCategoryComplete(cat_id))')
copy_branch = finalize.index('if(copy_items)', guard)
assert 'notifyLoadingFinished();' in finalize[guard:copy_branch]
assert 'return;' in finalize[guard:copy_branch]
assert finalize.index('getDirectDescendentsOf(cat_id') > copy_branch
assert finalize.index('createNewCategory(') > copy_branch
assert finalize.index('wearInventoryCategoryOnAvatar(cat, append)') > copy_branch

class_start = SOURCE.index('class CallAfterCategoryFetchStage1:')
class_open = SOURCE.index('{', class_start)
class_end, depth = class_open + 1, 1
while depth:
    depth += (SOURCE[class_end] == '{') - (SOURCE[class_end] == '}')
    class_end += 1
observer = SOURCE[class_start:class_end]
failure = observer.index('if (mComplete.size() <= 0)')
callback = observer.index('doOnIdleOneTime(mCallable)', failure)
assert callback > failure  # Failure callback still runs; wearCategoryFinal must fail closed.

wear = function(SOURCE, 'void LLAppearanceMgr::wearInventoryCategory(')
ais_branch = wear.index('if (AISAPI::isAvailable() && category->getPreferredType() == LLFolderType::FT_OUTFIT)')
ais_callback = wear.index('fetchFolderAndLinks(', ais_branch)
ais_lambda = wear.index('[cat_id, copy, append]', ais_callback)
ais_body = wear[ais_lambda:wear.index('});', ais_lambda)]
assert 'const LLUUID& fetched_id' in ais_body
assert 'fetched_id != cat_id' in ais_body
assert 'selfStopPhase("wear_inventory_category_fetch")' in ais_body
assert 'notifyLoadingFinished()' in ais_body

background_fetch = function(BACKGROUND_FETCH,
                            'void LLInventoryModelBackgroundFetch::fetchFolderAndLinks(')
assert 'fetchFolderAndLinks(cat_id, [callback](const LLUUID&)' in background_fetch
status_fetch = function(BACKGROUND_FETCH,
                        'void LLInventoryModelBackgroundFetch::fetchFolderAndLinks(const LLUUID& cat_id,\n                                                           fetch_completion_func_t callback)')
assert 'callback(id)' in status_fetch
assert '"Failed to fetch category links"' in status_fetch
assert '<< cat_id' not in status_fetch

ais_coro = function(AIS, 'void AISAPI::InvokeAISCommandCoro(')
status_guard = ais_coro.index('if (status && result.isMap())')
completion_guard = ais_coro.index('if (status && result.isMap())', status_guard + 1)
category_id = ais_coro.index('id = result["category_id"]', completion_guard)
assert status_guard < category_id
assert 'if (status && result.isMap())' in ais_coro
assert ais_coro.index('if (status && result.isMap())') < ais_coro.index('onUpdateReceived(result, type, body)')

assert '<< url' not in AIS
assert 'll_pretty_print_sd(result)' not in AIS
assert '<< result' not in AIS

print('PASS: failed category-fetch callbacks cannot copy or apply incomplete outfits')
