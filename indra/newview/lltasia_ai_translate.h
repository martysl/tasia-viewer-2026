#ifndef LL_TASIA_AI_TRANSLATE_H
#define LL_TASIA_AI_TRANSLATE_H
#include "lltranslate.h"
bool tasia_ai_translation_configured();
void tasia_ai_translate(const std::string& language, const std::string& text,
    LLTranslate::TranslationSuccess_fn success, LLTranslate::TranslationFailure_fn failure);
#endif
