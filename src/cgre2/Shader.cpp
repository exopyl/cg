#include "cgre2/Shader.hpp"
#include "cgre2/DeviceContext.hpp"
#include "cgre2/Logger.hpp"

#include <spirv_reflect.h>

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace cgre2 {

namespace {

[[nodiscard]] const char* spvReflectResultToString(SpvReflectResult r) {
    switch (r) {
        case SPV_REFLECT_RESULT_SUCCESS:                              return "SUCCESS";
        case SPV_REFLECT_RESULT_NOT_READY:                            return "NOT_READY";
        case SPV_REFLECT_RESULT_ERROR_PARSE_FAILED:                   return "PARSE_FAILED";
        case SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED:                   return "ALLOC_FAILED";
        case SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED:                 return "RANGE_EXCEEDED";
        case SPV_REFLECT_RESULT_ERROR_NULL_POINTER:                   return "NULL_POINTER";
        case SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR:                 return "INTERNAL_ERROR";
        case SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH:                 return "COUNT_MISMATCH";
        case SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND:              return "ELEMENT_NOT_FOUND";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_CODE_SIZE:        return "INVALID_CODE_SIZE";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_MAGIC_NUMBER:     return "INVALID_MAGIC_NUMBER";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF:           return "UNEXPECTED_EOF";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE:     return "INVALID_ID_REFERENCE";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_SET_NUMBER_OVERFLOW:      return "SET_NUMBER_OVERFLOW";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_STORAGE_CLASS:    return "INVALID_STORAGE_CLASS";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_RECURSION:                return "RECURSION";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_INSTRUCTION:      return "INVALID_INSTRUCTION";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_BLOCK_DATA:    return "UNEXPECTED_BLOCK_DATA";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_BLOCK_MEMBER_REFERENCE: return "INVALID_BLOCK_MEMBER_REFERENCE";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ENTRY_POINT:      return "INVALID_ENTRY_POINT";
        case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_EXECUTION_MODE:   return "INVALID_EXECUTION_MODE";
        default:                                                      return "UNKNOWN";
    }
}

void checkSpv(SpvReflectResult r, const char* what) {
    if (r != SPV_REFLECT_RESULT_SUCCESS) {
        const std::string err = std::string(what) + ": " + spvReflectResultToString(r);
        cgre2::Logger::error("Renderer", err);
        throw std::runtime_error(err);
    }
}

} // namespace

ShaderModule::ShaderModule(DeviceContext& device,
                           const std::string& spirvPath,
                           VkShaderStageFlagBits stage)
    : m_device(device), m_stage(stage), m_sourcePath(spirvPath) {
    const auto bytecode = readSpirv(spirvPath);

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = bytecode.size() * sizeof(uint32_t);
    info.pCode = bytecode.data();
    if (vkCreateShaderModule(m_device.getDevice(), &info, nullptr, &m_module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VkShaderModule: " + spirvPath);
    }

    reflect(bytecode);

    cgre2::Logger::info("Renderer",
        "Shader loaded: " + spirvPath +
        " (bindings=" + std::to_string(m_descriptorBindings.size()) +
        ", pushConstants=" + std::to_string(m_pushConstants.size()) +
        ", inputs=" + std::to_string(m_vertexInputs.size()) + ")");
}

ShaderModule::~ShaderModule() {
    if (m_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(m_device.getDevice(), m_module, nullptr);
        m_module = VK_NULL_HANDLE;
    }
}

std::vector<uint32_t> ShaderModule::readSpirv(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Cannot open SPIR-V file: " + path);
    }
    const auto sizeBytes = static_cast<size_t>(file.tellg());
    if (sizeBytes % sizeof(uint32_t) != 0) {
        throw std::runtime_error("SPIR-V file size not a multiple of 4: " + path);
    }
    std::vector<uint32_t> code(sizeBytes / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(sizeBytes));
    return code;
}

void ShaderModule::reflect(const std::vector<uint32_t>& spirv) {
    SpvReflectShaderModule mod{};
    checkSpv(spvReflectCreateShaderModule(spirv.size() * sizeof(uint32_t),
                                          spirv.data(), &mod),
             "spvReflectCreateShaderModule");

    if (mod.entry_point_name && *mod.entry_point_name) {
        m_entryPoint = mod.entry_point_name;
    }

    // Descriptor bindings
    {
        uint32_t count = 0;
        checkSpv(spvReflectEnumerateDescriptorBindings(&mod, &count, nullptr),
                 "enumerate descriptor bindings (count)");
        std::vector<SpvReflectDescriptorBinding*> refl(count);
        if (count > 0) {
            checkSpv(spvReflectEnumerateDescriptorBindings(&mod, &count, refl.data()),
                     "enumerate descriptor bindings");
        }
        m_descriptorBindings.reserve(count);
        for (auto* b : refl) {
            DescriptorBindingInfo info{};
            info.set             = b->set;
            info.binding         = b->binding;
            info.descriptorCount = std::max(b->count, 1u);
            info.descriptorType  = static_cast<VkDescriptorType>(b->descriptor_type);
            info.stageFlags      = static_cast<VkShaderStageFlags>(m_stage);
            info.name            = b->name ? b->name : "";
            m_descriptorBindings.push_back(std::move(info));
        }
    }

    // Push constant blocks
    {
        uint32_t count = 0;
        checkSpv(spvReflectEnumeratePushConstantBlocks(&mod, &count, nullptr),
                 "enumerate push constants (count)");
        std::vector<SpvReflectBlockVariable*> refl(count);
        if (count > 0) {
            checkSpv(spvReflectEnumeratePushConstantBlocks(&mod, &count, refl.data()),
                     "enumerate push constants");
        }
        m_pushConstants.reserve(count);
        for (auto* p : refl) {
            PushConstantInfo info{};
            info.offset     = p->offset;
            info.size       = p->size;
            info.stageFlags = static_cast<VkShaderStageFlags>(m_stage);
            m_pushConstants.push_back(info);
        }
    }

    // Vertex input variables (vertex stage only — other stages have
    // location inputs that map to inter-stage interfaces, not vertex
    // buffer attributes).
    if (m_stage == VK_SHADER_STAGE_VERTEX_BIT) {
        uint32_t count = 0;
        checkSpv(spvReflectEnumerateInputVariables(&mod, &count, nullptr),
                 "enumerate input variables (count)");
        std::vector<SpvReflectInterfaceVariable*> refl(count);
        if (count > 0) {
            checkSpv(spvReflectEnumerateInputVariables(&mod, &count, refl.data()),
                     "enumerate input variables");
        }
        m_vertexInputs.reserve(count);
        for (auto* in : refl) {
            // Skip built-ins (gl_VertexIndex etc.) — location is UINT32_MAX.
            if (in->location == UINT32_MAX) continue;
            VertexAttributeInfo info{};
            info.location = in->location;
            info.format   = static_cast<VkFormat>(in->format);
            info.name     = in->name ? in->name : "";
            m_vertexInputs.push_back(std::move(info));
        }
        // Sort by location so consumers can build attribute descriptions
        // deterministically.
        std::sort(m_vertexInputs.begin(), m_vertexInputs.end(),
                  [](const VertexAttributeInfo& a, const VertexAttributeInfo& b) {
                      return a.location < b.location;
                  });
    }

    spvReflectDestroyShaderModule(&mod);
}

// ShaderManager

ShaderManager::ShaderManager(DeviceContext& device) : m_device(device) {}

ShaderModule& ShaderManager::load(const std::string& spirvPath, VkShaderStageFlagBits stage) {
    auto it = m_cache.find(spirvPath);
    if (it != m_cache.end()) {
        return *it->second;
    }
    auto mod = std::make_unique<ShaderModule>(m_device, spirvPath, stage);
    auto& ref = *mod;
    m_cache.emplace(spirvPath, std::move(mod));
    return ref;
}

void ShaderManager::clear() {
    m_cache.clear();
}

} // namespace cgre2
