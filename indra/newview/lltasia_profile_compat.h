#pragma once
#include "llavatarpropertiesprocessor.h"

// Never reinterpret a smaller UDP reply as HTTP profile data. Only copy fields
// actually carried by AvatarPropertiesReply; groups/picks/notes are not supplied.
inline LLAvatarData tasiaLegacyProfileFields(const LLAvatarLegacyData& legacy)
{
    LLAvatarData result{};
    result.agent_id=legacy.agent_id; result.avatar_id=legacy.avatar_id;
    result.image_id=legacy.image_id; result.fl_image_id=legacy.fl_image_id;
    result.partner_id=legacy.partner_id; result.about_text=legacy.about_text;
    result.fl_about_text=legacy.fl_about_text; result.born_on=legacy.born_on;
    result.profile_url=legacy.profile_url; result.caption_index=legacy.caption_index;
    result.caption_text=legacy.caption_text; result.customer_type=legacy.customer_type;
    result.flags=legacy.flags;
    return result;
}
