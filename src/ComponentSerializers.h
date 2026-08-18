#pragma once

namespace rigkit {
namespace project {

class ComponentSerializerRegistry;

/** @brief Register rigComponent's portable POD codecs on a document registry. */
void registerComponentSerializers(ComponentSerializerRegistry& registry);

} // namespace project
} // namespace rigkit
