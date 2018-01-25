#include "pch.h"
#include "scene.h"

using namespace std;
using namespace wolf;
using namespace wolf::system;
using namespace wolf::graphics;

static uint32_t sFPS = 0;
static float sElapsedTimeInSec = 0;
static float sTotalTimeTimeInSec = 0;
//++++++++++++++++++++++++++++++++++++++++++++++++++++
//The following codes have been added for this project
//++++++++++++++++++++++++++++++++++++++++++++++++++++
static float sPush = true;
static float sPushConstantColorEdit[4] = { 0.4f, 0.7f, 0.0f, 1.0f };
//++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++

scene::scene(_In_z_ const std::wstring& pRunningDirectory, _In_z_ const std::wstring& pAppName) :
    w_game(pRunningDirectory, pAppName)
{
    auto _running_dir = pRunningDirectory;

#if defined(__WIN32) || defined(__UWP)
    content_path = _running_dir + L"../../../../content/";
#elif defined(__APPLE__)
    content_path = _running_dir + L"/../../../../../content/";
#elif defined(__linux)
    error
#elif defined(__ANDROID)
    error
#endif

#ifdef __WIN32
    w_graphics_device_manager_configs _config;
    _config.debug_gpu = false;
    w_game::set_graphics_device_manager_configs(_config);
#endif

    w_game::set_fixed_time_step(false);
}

scene::~scene()
{
	//release all resources
	release();
}

void scene::initialize(_In_ std::map<int, std::vector<w_window_info>> pOutputWindowsInfo)
{
	// TODO: Add your pre-initialization logic here

	w_game::initialize(pOutputWindowsInfo);
}

void scene::load()
{
    defer(nullptr, [&](...)
    {
        w_game::load();
    });

    const std::string _trace_info = this->name + "::load";

    auto _gDevice = this->graphics_devices[0];
    auto _output_window = &(_gDevice->output_presentation_windows[0]);

    w_point_t _screen_size;
    _screen_size.x = _output_window->width;
    _screen_size.y = _output_window->height;

    //initialize viewport
    this->_viewport.y = 0;
    this->_viewport.width = static_cast<float>(_screen_size.x);
    this->_viewport.height = static_cast<float>(_screen_size.y);
    this->_viewport.minDepth = 0;
    this->_viewport.maxDepth = 1;

    //initialize scissor of viewport
    this->_viewport_scissor.offset.x = 0;
    this->_viewport_scissor.offset.y = 0;
    this->_viewport_scissor.extent.width = _screen_size.x;
    this->_viewport_scissor.extent.height = _screen_size.y;

    //initialize attachment buffers
    w_attachment_buffer_desc _color(w_texture_buffer_type::W_TEXTURE_COLOR_BUFFER);
    w_attachment_buffer_desc _depth(w_texture_buffer_type::W_TEXTURE_DEPTH_BUFFER);

    //define color and depth buffers for render pass
    std::vector<w_attachment_buffer_desc> _attachment_descriptions = { _color, _depth };

    //create render pass
    auto _hr = this->_draw_render_pass.load(_gDevice,
        _viewport,
        _viewport_scissor,
        _attachment_descriptions);
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "creating render pass", _trace_info, 3, true);
    }

    //create frame buffers
    auto _draw_render_pass_handle = this->_draw_render_pass.get_handle();
    _hr = this->_draw_frame_buffers.load(_gDevice,
        _draw_render_pass_handle,
        _output_window);
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "creating frame buffers", _trace_info, 3, true);
    }

    //create semaphore
    _hr = this->_draw_semaphore.initialize(_gDevice);
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "creating draw semaphore", _trace_info, 3, true);
    }

    //Fence for syncing
    _hr = this->_draw_fence.initialize(_gDevice);
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "creating draw fence", _trace_info, 3, true);
    }

    //load imgui
	w_imgui::load(
		_gDevice,
		_output_window,
		this->_viewport,
		this->_viewport_scissor,
		nullptr);

    //create two primary command buffers for clearing screen
    auto _swap_chain_image_size = _output_window->vk_swap_chain_image_views.size();
    _hr = this->_draw_command_buffers.load(_gDevice, _swap_chain_image_size);
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "creating draw command buffers", _trace_info, 3, true);
    }

#ifdef WIN32
    auto _content_path_dir = wolf::system::io::get_current_directory() + L"/../../../../samples/02_basics/12_push_constants/src/content/";
#elif defined(__APPLE__)
    auto _content_path_dir = wolf::system::io::get_current_directory() + L"/../../../../../samples/02_basics/12_push_constants/src/content/";
#endif // WIN32

    //loading vertex shaders
    _hr = this->_shader.load(_gDevice,
        _content_path_dir + L"shaders/shader.vert.spv",
        w_shader_stage::VERTEX_SHADER);
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "loading vertex shader", _trace_info, 3, true);
    }

    //loading fragment shader
    _hr = this->_shader.load(_gDevice,
        _content_path_dir + L"shaders/shader.frag.spv",
        w_shader_stage::FRAGMENT_SHADER);
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "loading fragment shader", _trace_info, 3, true);
    }

    //load texture
    _hr = this->_texture.initialize(_gDevice);
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "loading texture", _trace_info, 3, true);
    }
    //load texture from file
    _hr = this->_texture.load_texture_2D_from_file(content_path + L"../Logo.jpg", true);
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "loading Logo.jpg texture", _trace_info, 3, true);
    }

    //just we need vertex position color
    this->_mesh.set_vertex_binding_attributes(w_vertex_declaration::VERTEX_POSITION_UV);

    w_shader_binding_param _shader_param;
    _shader_param.index = 0;
    _shader_param.type = w_shader_binding_type::SAMPLER2D;
    _shader_param.stage = w_shader_stage::FRAGMENT_SHADER;
    _shader_param.image_info = this->_texture.get_descriptor_info();

    _hr = this->_shader.set_shader_binding_params(
    {
        _shader_param
    });
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "setting shader binding param", _trace_info, 3, true);
    }

    //loading pipeline cache
    std::string _pipeline_cache_name = "pipeline_cache";
    if (w_pipeline::create_pipeline_cache(_gDevice, _pipeline_cache_name) == S_FALSE)
    {
        logger.error("could not create pipeline cache");
        _pipeline_cache_name.clear();
    }

    VkPushConstantRange _push_constants_buffer_range;
    _push_constants_buffer_range.offset = 0;
    _push_constants_buffer_range.size = static_cast<uint32_t>(4 * sizeof(float));
    _push_constants_buffer_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    auto _descriptor_set_layout_binding = this->_shader.get_descriptor_set_layout();
    _hr = this->_pipeline.load(_gDevice,
        this->_mesh.get_vertex_binding_attributes(),
        VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        this->_draw_render_pass.get_handle(),
        this->_shader.get_shader_stages(),
        _descriptor_set_layout_binding ? &_descriptor_set_layout_binding : nullptr,
        { this->_viewport },
        { this->_viewport_scissor },
        "pipeline_cache",
        {},
        { _push_constants_buffer_range });
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "creating solid pipeline", _trace_info, 3, true);
    }

    std::vector<float> _vertex_data =
    {
        -0.7f, -0.7f,	0.0f,		//pos0
         0.0f,  0.0f,               //uv0
        -0.7f,  0.7f,	0.0f,		//pos1
         0.0f,  1.0f,               //uv1
         0.7f,  0.7f,	0.0f,		//pos2
         1.0f,  1.0f,           	//uv2
         0.7f, -0.7f,	0.0f,		//pos3
         1.0f,  0.0f,               //uv3
    };

    std::vector<uint32_t> _index_data =
    {
        0,
        1,
        3,
        3,
        1,
        2
    };

    this->_mesh.set_texture(&this->_texture);
    _hr = this->_mesh.load(_gDevice,
        _vertex_data.data(),
        static_cast<uint32_t>(_vertex_data.size() * sizeof(float)),
        static_cast<uint32_t>(_vertex_data.size()),
        _index_data.data(),
        static_cast<uint32_t>(_index_data.size()));
    if (_hr == S_FALSE)
    {
        release();
        V(S_FALSE, "loading mesh", _trace_info, 3, true);
    }
}

HRESULT scene::_build_draw_command_buffers()
{
    const std::string _trace_info = this->name + "::build_draw_command_buffers";
    HRESULT _hr = S_OK;

    auto _size = this->_draw_command_buffers.get_commands_size();
    for (uint32_t i = 0; i < _size; ++i)
    {
        this->_draw_command_buffers.begin(i);
        {
            auto _frame_buffer_handle = this->_draw_frame_buffers.get_frame_buffer_at(i);

            auto _cmd = this->_draw_command_buffers.get_command_at(i);
            this->_draw_render_pass.begin(_cmd,
                _frame_buffer_handle,
                w_color::CORNFLOWER_BLUE(),
                1.0f,
                0.0f);
            {
                auto _description_set = this->_shader.get_descriptor_set();

                //++++++++++++++++++++++++++++++++++++++++++++++++++++
                //The following codes have been added for this project
                //++++++++++++++++++++++++++++++++++++++++++++++++++++
                vkCmdPushConstants(
                    _cmd,
                    this->_pipeline.get_layout_handle(),
                    VK_SHADER_STAGE_VERTEX_BIT,
                    0,
                    static_cast<uint32_t>(4 * sizeof(float)),
                    &sPushConstantColorEdit);
                //++++++++++++++++++++++++++++++++++++++++++++++++++++
                //++++++++++++++++++++++++++++++++++++++++++++++++++++

                this->_pipeline.bind(_cmd, &_description_set);

                _hr = this->_mesh.draw(_cmd, nullptr, 0, false);
                if (_hr == S_FALSE)
                {
                    V(S_FALSE, "drawing mesh", _trace_info, 3, false);
                }
            }
            this->_draw_render_pass.end(_cmd);
        }
        this->_draw_command_buffers.end(i);
    }
    return _hr;
}

void scene::update(_In_ const wolf::system::w_game_time& pGameTime)
{
	if (w_game::exiting) return;
	const std::string _trace_info = this->name + "::update";

    sFPS = pGameTime.get_frames_per_second();
    sElapsedTimeInSec = pGameTime.get_elapsed_seconds();
    sTotalTimeTimeInSec = pGameTime.get_total_seconds();

    w_imgui::new_frame(sElapsedTimeInSec, [this]()
    {
        _update_gui();
    });
    
	w_game::update(pGameTime);
}

HRESULT scene::render(_In_ const wolf::system::w_game_time& pGameTime)
{
	if (w_game::exiting) return S_OK;

	const std::string _trace_info = this->name + "::render";

	auto _gDevice = this->graphics_devices[0];
	auto _output_window = &(_gDevice->output_presentation_windows[0]);
	auto _frame_index = _output_window->vk_swap_chain_image_index;

    if (sPush)
    {
        _build_draw_command_buffers();
        sPush = false;
    }

	w_imgui::render();

	//add wait semaphores
	std::vector<VkSemaphore> _wait_semaphors = { *(_output_window->vk_swap_chain_image_is_available_semaphore.get()) };
	auto _draw_command_buffer = this->_draw_command_buffers.get_command_at(_frame_index);
	auto _gui_command_buffer = w_imgui::get_command_buffer_at(_frame_index);

	const VkPipelineStageFlags _wait_dst_stage_mask[] =
	{
		VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
	};

	//reset draw fence
	this->_draw_fence.reset();
	if (_gDevice->submit(
		{ _draw_command_buffer, _gui_command_buffer },
		_gDevice->vk_graphics_queue,
		&_wait_dst_stage_mask[0],
		_wait_semaphors,
		{ *_output_window->vk_rendering_done_semaphore.get() },
		&this->_draw_fence) == S_FALSE)
	{
		V(S_FALSE, "submiting queue for drawing", _trace_info, 3, true);
	}
	this->_draw_fence.wait();

	//clear all wait semaphores
	_wait_semaphors.clear();

	return w_game::render(pGameTime);
}

void scene::on_window_resized(_In_ uint32_t pIndex)
{
	w_game::on_window_resized(pIndex);
}

void scene::on_device_lost()
{
	w_game::on_device_lost();
}

ULONG scene::release()
{
    if (this->get_is_released()) return 1;

    //release draw's objects
	this->_draw_fence.release();
	this->_draw_semaphore.release();

	this->_draw_command_buffers.release();
	this->_draw_render_pass.release();
	this->_draw_frame_buffers.release();

    //release gui's objects
    w_imgui::release();

	this->_shader.release();

    this->_pipeline.release();
    
	this->_mesh.release();
    this->_texture.release();

	return w_game::release();
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++
//The following codes have been added for this project
//++++++++++++++++++++++++++++++++++++++++++++++++++++
bool scene::_update_gui()
{
    //Setting Style
    ImGuiStyle& _style = ImGui::GetStyle();
    _style.Colors[ImGuiCol_Text].x = 1.0f;
    _style.Colors[ImGuiCol_Text].y = 1.0f;
    _style.Colors[ImGuiCol_Text].z = 1.0f;
    _style.Colors[ImGuiCol_Text].w = 1.0f;

    _style.Colors[ImGuiCol_WindowBg].x = 0.0f;
    _style.Colors[ImGuiCol_WindowBg].y = 0.4f;
    _style.Colors[ImGuiCol_WindowBg].z = 1.0f;
    _style.Colors[ImGuiCol_WindowBg].w = 1.0f;

    ImGuiWindowFlags  _window_flags = 0;;
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiSetCond_FirstUseEver);
    bool _is_open = true;
    if (!ImGui::Begin("Wolf.Engine", &_is_open, _window_flags))
    {
        ImGui::End();
        return false;
    }

    ImGui::Text("Press Esc to exit\r\nFPS:%d\r\nFrameTime:%f\r\nTotalTime:%f\r\nMouse Position:%d,%d\r\n",
        sFPS,
        sElapsedTimeInSec,
        sTotalTimeTimeInSec,
        wolf::inputs_manager.mouse.pos_x, wolf::inputs_manager.mouse.pos_y);

    if (ImGui::ColorEdit4("Pick Color", sPushConstantColorEdit))
    {
        sPush = true;
    }

    ImGui::End();

    return true;
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++
//++++++++++++++++++++++++++++++++++++++++++++++++++++