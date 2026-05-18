#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cgre2 {

class VulkanDevice;

/// Single descriptor binding extracted from SPIR-V reflection.
/// `stageFlags` is set to the owning shader's stage; the layout builder
/// fuses entries that share (set, binding) across stages by OR-ing this
/// field, so individual ShaderModules only ever populate one bit.
struct DescriptorBindingInfo {
    uint32_t           set = 0;
    uint32_t           binding = 0;
    uint32_t           descriptorCount = 1;
    VkDescriptorType   descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
    VkShaderStageFlags stageFlags = 0;
    std::string        name;
};

/// Push constant range extracted from a shader's `layout(push_constant)`
/// block. Aggregation across stages is the layout builder's job.
struct PushConstantInfo {
    uint32_t           offset = 0;
    uint32_t           size = 0;
    VkShaderStageFlags stageFlags = 0;
};

/// One `layout(location=N) in <T> name;` entry of a vertex shader.
/// Populated only for VK_SHADER_STAGE_VERTEX_BIT modules.
struct VertexAttributeInfo {
    uint32_t    location = 0;
    VkFormat    format = VK_FORMAT_UNDEFINED;
    std::string name;
};

/// Owns a single `VkShaderModule` and the metadata derived from its SPIR-V
/// via SPIRV-Reflect. The bytecode is not retained past construction —
/// reflection is run once, then the source buffer is dropped.
///
/// Lifetime: must outlive any VkPipeline that references its handle.
class ShaderModule {
public:
    /// Read `spirvPath`, create the Vulkan module, run reflection.
    /// @throws std::runtime_error on I/O, validation or reflection failure.
    ShaderModule(VulkanDevice& device,
                 const std::string& spirvPath,
                 VkShaderStageFlagBits stage);
    ~ShaderModule();

    ShaderModule(const ShaderModule&) = delete;
    ShaderModule& operator=(const ShaderModule&) = delete;
    ShaderModule(ShaderModule&&) = delete;
    ShaderModule& operator=(ShaderModule&&) = delete;

    [[nodiscard]] VkShaderModule        getHandle()     const { return m_module; }
    [[nodiscard]] VkShaderStageFlagBits getStage()      const { return m_stage; }
    [[nodiscard]] const std::string&    getEntryPoint() const { return m_entryPoint; }
    [[nodiscard]] const std::string&    getSourcePath() const { return m_sourcePath; }

    [[nodiscard]] const std::vector<DescriptorBindingInfo>& getDescriptorBindings() const { return m_descriptorBindings; }
    [[nodiscard]] const std::vector<PushConstantInfo>&      getPushConstants()      const { return m_pushConstants; }
    [[nodiscard]] const std::vector<VertexAttributeInfo>&   getVertexInputs()       const { return m_vertexInputs; }

private:
    void reflect(const std::vector<uint32_t>& spirv);
    [[nodiscard]] static std::vector<uint32_t> readSpirv(const std::string& path);

    VulkanDevice&         m_device;
    VkShaderModule        m_module = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_stage;
    std::string           m_entryPoint = "main";
    std::string           m_sourcePath;

    std::vector<DescriptorBindingInfo> m_descriptorBindings;
    std::vector<PushConstantInfo>      m_pushConstants;
    std::vector<VertexAttributeInfo>   m_vertexInputs;
};

/// Cache of ShaderModule keyed by canonical SPIR-V path. Cheap copies for
/// callers (Pipelines hold raw references). Lifetime: typically owned by
/// the application, destroyed AFTER all pipelines that reference its
/// modules.
class ShaderManager {
public:
    explicit ShaderManager(VulkanDevice& device);
    ~ShaderManager() = default;

    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager(ShaderManager&&) = delete;
    ShaderManager& operator=(ShaderManager&&) = delete;

    /// Load `spirvPath` for `stage` (or return the cached module). Same
    /// path called twice with a different stage is a programming error —
    /// callers should keep one Manager per intended layout.
    ShaderModule& load(const std::string& spirvPath, VkShaderStageFlagBits stage);

    /// Drop every cached module. Safe only when no live pipeline still
    /// references their handles.
    void clear();

private:
    VulkanDevice& m_device;
    std::unordered_map<std::string, std::unique_ptr<ShaderModule>> m_cache;
};

} // namespace cgre2
