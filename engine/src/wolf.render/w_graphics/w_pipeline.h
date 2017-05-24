/*
	Project			 : Wolf Engine. Copyright(c) Pooya Eimandar (http://PooyaEimandar.com) . All rights reserved.
	Source			 : Please direct any bug to https://github.com/PooyaEimandar/Wolf.Engine/issues
	Website			 : http://WolfSource.io
	Name			 : w_pipeline.h
	Description		 : create pipeline
	Comment          :
*/

#ifndef __W_PIEPELINE_H__
#define __W_PIEPELINE_H__

#include "w_graphics_device_manager.h"

namespace wolf
{
	namespace graphics
	{
        class w_pipeline_pimp;
		class w_pipeline : public system::w_object
		{
		public:
			W_EXP w_pipeline();
			W_EXP virtual ~w_pipeline();

            W_EXP HRESULT load(_In_ const std::shared_ptr<w_graphics_device>& pGDevice,
                               _In_ const std::string& pPipelineCacheName,
                               _In_ const VkRenderPass pRenderPass,
                               _In_ const std::vector<VkPipelineShaderStageCreateInfo>* pShaderStages,
                               _In_ const std::vector<w_viewport> pViewPorts,
                               _In_ const std::vector<w_viewport_scissor> pViewPortsScissors,
                               _In_ const VkPipelineLayoutCreateInfo* const pPipelineLayoutCreateInfo = nullptr,
                               _In_ const VkPipelineVertexInputStateCreateInfo* const pPipelineVertexInputStateCreateInfo = nullptr,
                               _In_ const VkPipelineInputAssemblyStateCreateInfo* const pPipelineInputAssemblyStateCreateInfo = nullptr,
                               _In_ const VkPipelineRasterizationStateCreateInfo* const pPipelineRasterizationStateCreateInfo = nullptr,
                               _In_ const VkPipelineMultisampleStateCreateInfo* const pPipelineMultisampleStateCreateInfo = nullptr,
                               _In_ const VkPipelineDynamicStateCreateInfo* const pPipelineDynamicStateCreateInfo = nullptr,
                               _In_ const bool pEnableDepthStencilState = true,
                               _In_ const VkPipelineColorBlendAttachmentState pBlendState = w_graphics_device::w_blend_states::premulitplied_alpha,
                               _In_ const std::array<float,4> pBlendColors = { 0.0f, 0.0f, 0.0f, 0.0f });
            
            //release all resources
            W_EXP virtual ULONG release() override;
            
            
#pragma region Getters

			W_EXP const VkPipeline get_handle() const;
			W_EXP const VkPipelineLayout get_layout_handle() const;
            
#pragma endregion

            W_EXP static VkPipelineLayout create_pipeline_layout(_In_ const std::shared_ptr<w_graphics_device>& pGDevice,
                _In_ const VkPipelineLayoutCreateInfo* const pPipelineLayoutCreateInfo);

            W_EXP static HRESULT create_pipeline_cache(_In_ const std::shared_ptr<w_graphics_device>& pGDevice, 
                _In_z_ const std::string& pPipelineCacheName);
            
            W_EXP static VkPipelineCache get_pipeline_cache(_In_z_ const std::string& pPipelineCacheName);
            
            W_EXP static ULONG release_all_pipeline_caches(_In_ const std::shared_ptr<w_graphics_device>& pGDevice);

        private:
			typedef system::w_object						_super;
            w_pipeline_pimp*                                _pimp;
        };
    }
}

#endif
