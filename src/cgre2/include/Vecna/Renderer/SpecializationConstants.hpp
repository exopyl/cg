#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace cgre2 {

/// Accumulator for `layout(constant_id = N) const T name;` values, packed
/// into a single VkSpecializationInfo at pipeline-creation time.
///
/// Vulkan ignores specialization entries whose constantID is not declared
/// in the target shader, so it's safe to share one instance across
/// vertex/fragment stages even if only one of them references a given
/// constant.
///
/// Lifetime: the VkSpecializationInfo returned by build() references this
/// object's internal vectors. Keep the SpecializationConstants alive at
/// least until vkCreateGraphicsPipelines has consumed the info struct.
class SpecializationConstants {
public:
    SpecializationConstants() = default;

    // Booleans must be stored as VkBool32 per the Vulkan spec.
    void setBool(uint32_t constantID, bool value) {
        const VkBool32 v = value ? VK_TRUE : VK_FALSE;
        store(constantID, &v, sizeof(v));
    }

    void setInt32 (uint32_t constantID, int32_t  value) { store(constantID, &value, sizeof(value)); }
    void setUint32(uint32_t constantID, uint32_t value) { store(constantID, &value, sizeof(value)); }
    void setFloat (uint32_t constantID, float    value) { store(constantID, &value, sizeof(value)); }

    /// Build a VkSpecializationInfo backed by this object's storage. The
    /// returned struct's pointers MUST NOT outlive `*this`.
    [[nodiscard]] VkSpecializationInfo build() const {
        VkSpecializationInfo info{};
        info.mapEntryCount = static_cast<uint32_t>(m_entries.size());
        info.pMapEntries   = m_entries.data();
        info.dataSize      = m_data.size();
        info.pData         = m_data.data();
        return info;
    }

    [[nodiscard]] bool empty() const { return m_entries.empty(); }

private:
    void store(uint32_t constantID, const void* data, size_t size) {
        VkSpecializationMapEntry entry{};
        entry.constantID = constantID;
        entry.offset     = static_cast<uint32_t>(m_data.size());
        entry.size       = size;
        m_entries.push_back(entry);

        const auto oldSize = m_data.size();
        m_data.resize(oldSize + size);
        std::memcpy(m_data.data() + oldSize, data, size);
    }

    std::vector<VkSpecializationMapEntry> m_entries;
    std::vector<std::byte>                m_data;
};

} // namespace cgre2
