#include "cgre2/Pipeline.hpp"
#include "cgre2/Shader.hpp"
#include "cgre2/SpecializationConstants.hpp"
#include "cgre2/DeviceContext.hpp"
#include "cgre2/Logger.hpp"

#include <array>
#include <stdexcept>

namespace cgre2 {

Pipeline::Pipeline(DeviceContext& device, const PipelineCreateInfo& info)
    : m_device(device) {
    if (!info.vertexShader || !info.fragmentShader) {
        throw std::runtime_error("PipelineCreateInfo: vertex and fragment shaders are required");
    }
    if (!info.vertexLayout) {
        throw std::runtime_error("PipelineCreateInfo: vertexLayout is required");
    }
    if (info.renderPass == VK_NULL_HANDLE) {
        throw std::runtime_error("PipelineCreateInfo: renderPass is required");
    }

    createPipelineLayout(info);
    createGraphicsPipeline(info);
}

Pipeline::~Pipeline() {
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device.getDevice(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device.getDevice(), m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    cgre2::Logger::info("Renderer", "Graphics pipeline destroyed");
}

void Pipeline::createPipelineLayout(const PipelineCreateInfo& info) {
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = static_cast<uint32_t>(info.descriptorSetLayouts.size());
    layoutInfo.pSetLayouts            = info.descriptorSetLayouts.empty()
                                            ? nullptr
                                            : info.descriptorSetLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(info.pushConstantRanges.size());
    layoutInfo.pPushConstantRanges    = info.pushConstantRanges.empty()
                                            ? nullptr
                                            : info.pushConstantRanges.data();

    if (vkCreatePipelineLayout(m_device.getDevice(), &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        cgre2::Logger::error("Renderer", "Failed to create pipeline layout");
        throw std::runtime_error("Failed to create pipeline layout");
    }
    cgre2::Logger::info("Renderer", "Pipeline layout created");
}

void Pipeline::createGraphicsPipeline(const PipelineCreateInfo& info) {
    // Specialization info backs into the SpecializationConstants object —
    // we keep it on the stack here for the lifetime of this call so the
    // pointers it carries stay valid until vkCreateGraphicsPipelines
    // consumes the data.
    VkSpecializationInfo specInfo{};
    const bool hasSpec = (info.specialization != nullptr) && !info.specialization->empty();
    if (hasSpec) {
        specInfo = info.specialization->build();
    }

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = info.vertexShader->getHandle();
    vertStage.pName  = info.vertexShader->getEntryPoint().c_str();
    if (hasSpec) vertStage.pSpecializationInfo = &specInfo;

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = info.fragmentShader->getHandle();
    fragStage.pName  = info.fragmentShader->getEntryPoint().c_str();
    if (hasSpec) fragStage.pSpecializationInfo = &specInfo;

    const std::array<VkPipelineShaderStageCreateInfo, 2> stages = { vertStage, fragStage };

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &info.vertexLayout->binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(info.vertexLayout->attributes.size());
    vertexInput.pVertexAttributeDescriptions    = info.vertexLayout->attributes.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = info.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport / scissor — dynamic, count only.
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = info.polygonMode;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = info.cullMode;
    rasterizer.frontFace               = info.frontFace;
    rasterizer.depthBiasEnable         = VK_FALSE;

    // Multisampling (disabled)
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth/stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = info.depthTest  ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable      = info.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp        = info.depthCompareOp;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = VK_FALSE;

    // Color blending (opaque)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable   = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    // Dynamic states (viewport + scissor)
    constexpr std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates    = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages             = stages.data();
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass          = info.renderPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex   = -1;

    if (vkCreateGraphicsPipelines(m_device.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        cgre2::Logger::error("Renderer", "Failed to create graphics pipeline");
        throw std::runtime_error("Failed to create graphics pipeline");
    }
    cgre2::Logger::info("Renderer", "Graphics pipeline created");
}

} // namespace cgre2
