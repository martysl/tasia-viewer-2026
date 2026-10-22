#!/usr/bin/env python3
"""Verify incomplete category fetches cannot proceed to outfit mutation."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = (ROOT / 'indra/newview/llappearancemgr.cpp').read_text()


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
print('PASS: failed category-fetch callbacks cannot copy or apply incomplete outfits')
