#include "w_render_pch.h"
#include "w_buffer.h"
#include <w_convert.h>

namespace wolf
{
    namespace graphics
    {
        class w_buffer_pimp
        {
        public:
            w_buffer_pimp() :
                _gDevice(nullptr),
                _size(0),
                _handle(0),
                _memory(0)
            {
            }
            
            HRESULT load(_In_ const std::shared_ptr<w_graphics_device>& pGDevice,
                         _In_ const uint32_t pBufferSize,
                         _In_ const VkBufferUsageFlags pUsageFlags,
                         _In_ const VkMemoryPropertyFlags pMemoryFlags)
            {
                this->_gDevice = pGDevice;
                this->_size = pBufferSize;
                this->_usage_flags = pUsageFlags;
                this->_memory_flags = pMemoryFlags;
                
                const VkBufferCreateInfo _buffer_create_info =
                {
                    VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,             // Type
                    nullptr,                                          // Next
                    0,                                                // Flags
                    (VkDeviceSize)this->_size,                        // Size
                    this->_usage_flags,                                           // Usage
                    VK_SHARING_MODE_EXCLUSIVE,                        // SharingMode
                    0,                                                // QueueFamilyIndexCount
                    nullptr                                           // QueueFamilyIndices
                };
                
                auto _hr = vkCreateBuffer(this->_gDevice->vk_device,
                                          &_buffer_create_info,
                                          nullptr,
                                          &this->_handle);
                if(_hr)
                {
                    V(S_FALSE, "w_buffer", "creating buffer for graphics device: " + this->_gDevice->device_name +
                      " ID: " + std::to_string(this->_gDevice->device_id), 3, false, true);
                    return S_FALSE;
                }
                
                VkMemoryRequirements _buffer_memory_requirements;
                vkGetBufferMemoryRequirements(this->_gDevice->vk_device,
                                              this->_handle,
                                              &_buffer_memory_requirements);
                
                VkPhysicalDeviceMemoryProperties _memory_properties;
                vkGetPhysicalDeviceMemoryProperties(this->_gDevice->vk_physical_device,
                                                    &_memory_properties);
                
                for( uint32_t i = 0; i < _memory_properties.memoryTypeCount; ++i )
                {
                    if( (_buffer_memory_requirements.memoryTypeBits & (1 << i)) &&
                       (_memory_properties.memoryTypes[i].propertyFlags & this->_memory_flags) )
                    {
                        
                        VkMemoryAllocateInfo _memory_allocate_info =
                        {
                            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, // Type
                            nullptr,                                // Next
                            _buffer_memory_requirements.size,       // AllocationSize
                            i                                       // MemoryTypeIndex
                        };
                        
                        if( vkAllocateMemory(this->_gDevice->vk_device,
                                             &_memory_allocate_info,
                                             nullptr,
                                             &this->_memory) == VK_SUCCESS)
                        {
                            return S_OK;
                        }
                    }
                }
                
                logger.error("Could not create buffer, because proposed memory property not found.");
                
                return S_FALSE;
            }
            
            //Set data to DRAM
            HRESULT set_data(_In_ const void* const pData)
            {
                //we can not access to VRAM, but we can copy our data to DRAM
                if (this->_memory_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) return S_FALSE;
                
                auto _size = this->get_size();
                auto _memory = this->get_memory();
                
                void* _data = nullptr;
                auto _hr = vkMapMemory(this->_gDevice->vk_device,
                                       _memory,
                                       0,
                                       _size,
                                       0,
                                       &_data);
                if(_hr)
                {
                    _data = nullptr;
                    V(S_FALSE, "mapping data to to vertex buffer's memory for graphics device: " +
                      _gDevice->device_name + " ID:" + std::to_string(_gDevice->device_id),
                      this->_name, 3, false, true);
                    
                    return S_FALSE;
                }
                
                memcpy(_data, pData, (size_t)_size);
                
                VkMappedMemoryRange _flush_range =
                {
                    VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,            // Type
                    nullptr,                                          // Next
                    _memory,                                          // Memory
                    0,                                                // Offset
                    VK_WHOLE_SIZE                                     // Size
                };
                
                vkFlushMappedMemoryRanges(this->_gDevice->vk_device, 1, &_flush_range);
                
                //unmap memory
                vkUnmapMemory(this->_gDevice->vk_device, _memory);
                
                _data = nullptr;
                
                return S_OK;
            }
            
            HRESULT bind()
            {
                return vkBindBufferMemory(this->_gDevice->vk_device,
                                          this->_handle,
                                          this->_memory,
                                          0) == VK_SUCCESS ? S_OK : S_FALSE;
            }
            
            ULONG release()
            {
                this->_size = 0;
                
                if(this->_handle)
                {
                    vkDestroyBuffer(this->_gDevice->vk_device,
                                    this->_handle,
                                    nullptr);
                    
                    this->_handle = 0;
                }
                
                if(this->_memory)
                {
                    vkFreeMemory(this->_gDevice->vk_device,
                                 this->_memory,
                                 nullptr);
                    
                    this->_memory = 0;
                }

                this->_gDevice = nullptr;
                
                return 1;
            }
            
            const UINT32 get_size() const
            {
                return this->_size;
            }
            
            const VkBufferUsageFlags get_usage_flags() const
            {
                return this->_usage_flags;
            }
            
            const VkMemoryPropertyFlags get_memory_flags() const
            {
                return this->_memory_flags;
            }
            
            const VkBuffer get_handle() const
            {
                return this->_handle;
            }
            
            const VkDeviceMemory get_memory() const
            {
                return this->_memory;
            }
            
        private:
            std::string                                         _name;
            std::shared_ptr<w_graphics_device>                  _gDevice;
            UINT32                                              _size;
            
#ifdef __VULKAN__
            VkBuffer                                            _handle;
            VkDeviceMemory                                      _memory;
            VkMemoryPropertyFlags                               _memory_flags;
            VkBufferUsageFlags                                  _usage_flags;
#endif
            
        };
    }
}

using namespace wolf::graphics;

w_buffer::w_buffer() : _pimp(new w_buffer_pimp())
{
    _super::set_class_name("w_buffer");
}

w_buffer::~w_buffer()
{
    release();
}

HRESULT w_buffer::load_as_staging(_In_ const std::shared_ptr<w_graphics_device>& pGDevice,
                                  _In_ const uint32_t pBufferSize)
{
    if(!this->_pimp) return S_FALSE;
    
    return this->_pimp->load(pGDevice,
                             pBufferSize,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

HRESULT w_buffer::load(_In_ const std::shared_ptr<w_graphics_device>& pGDevice,
                       _In_ const uint32_t pBufferSize,
                       _In_ const VkBufferUsageFlags pUsageFlags,
                       _In_ const VkMemoryPropertyFlags pMemoryFlags)
{
    if(!this->_pimp) return S_FALSE;
    
    return this->_pimp->load(pGDevice, pBufferSize, pUsageFlags, pMemoryFlags);
}

HRESULT w_buffer::set_data(_In_ const void* const pData)
{
    if(!this->_pimp) return S_FALSE;
    
    return this->_pimp->set_data(pData);
}

HRESULT w_buffer::bind()
{
    if(!this->_pimp) return S_FALSE;
    
    return this->_pimp->bind();
}

ULONG w_buffer::release()
{
    if(_super::get_is_released()) return 0;
    
    SAFE_RELEASE(this->_pimp);
    
    return _super::release();
}

#pragma region Getters

const UINT32 w_buffer::get_size() const
{
    if(!this->_pimp) return 0;
    
    return this->_pimp->get_size();
}

const VkBufferUsageFlags w_buffer::get_usage_flags() const
{
    if(!this->_pimp) return VkBufferUsageFlagBits::VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM;
    
    return this->_pimp->get_usage_flags();
}

const VkMemoryPropertyFlags w_buffer::get_memory_flags() const
{
    if(!this->_pimp) return VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    return this->_pimp->get_memory_flags();
}

const VkBuffer w_buffer::get_handle() const
{
    if(!this->_pimp) return 0;
    
    return this->_pimp->get_handle();
}

const VkDeviceMemory w_buffer::get_memory() const
{
    if(!this->_pimp) return 0;
    
    return this->_pimp->get_memory();
}

#pragma endregion

