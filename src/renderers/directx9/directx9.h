#pragma once
#pragma comment (lib,"d3d9.lib")
#include <d3d9.h>

#include <null-render.h>

namespace null::render::directx9 {
	struct vertex_t {
		float pos[3]{ };
		std::uint32_t color{ };
		float uv[2]{ };
	};

	inline IDirect3DDevice9* device{ };
	inline IDirect3DTexture9* font_texture{ };
	inline IDirect3DVertexDeclaration9* vtx_declaration{ };
	inline IDirect3DVertexBuffer9* vtx_buffer{ };
	inline IDirect3DIndexBuffer9* idx_buffer{ };

	void render_draw_data(c_draw_list::draw_data_t* _draw_data = &draw_data);
	void setup_render_state(c_draw_list::draw_data_t* _draw_data = &draw_data);
	void create_fonts_texture();
	void create_device_objects();
	void invalidate_device_objects();

	static void initialize(IDirect3DDevice9* _device) { device = _device; device->AddRef(); }
	static void shutdown() { invalidate_device_objects(); if(device) { device->Release(); device = nullptr; } }

	static void begin_frame() { if(!font_texture) create_device_objects(); }
	
	class c_window : public utils::win::c_window {
	public:
		color_t clear_color{ 18, 18, 18 };
		IDirect3D9* direct3d{ };
		D3DPRESENT_PARAMETERS present_parameters{
			.BackBufferFormat{ D3DFMT_UNKNOWN },
			.SwapEffect{ D3DSWAPEFFECT_DISCARD },
			.Windowed{ true },
			.EnableAutoDepthStencil{ true },
			.AutoDepthStencilFormat{ D3DFMT_D16 },
			.PresentationInterval{ D3DPRESENT_INTERVAL_ONE }
		};

	public:
		c_window() = default;
		c_window(HINSTANCE _instance) : utils::win::c_window{ _instance } { }

		void render_create() override {
			if(!(direct3d = Direct3DCreate9(D3D_SDK_VERSION)))
				throw std::runtime_error("cannot create direct3d");
			if(direct3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd_handle, D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device) < 0)
				throw std::runtime_error("cannot create device");
		}

		void render_destroy() override {
			if(device) { device->Release(); device = nullptr; }
			if(direct3d) { direct3d->Release(); direct3d = nullptr; }
		}

		void render_main_loop_begin() override { begin_frame(); }
		void render_main_loop_end() override {
			setup_draw_data();

			device->SetRenderState(D3DRS_ZENABLE, FALSE);
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
			std::uint32_t clr = (std::uint32_t)clear_color;
			device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, (clr & 0xFF00FF00) | ((clr & 0xFF0000) >> 16) | ((clr & 0xFF) << 16), 1.0f, 0);
			if(device->BeginScene() >= 0) {
				render_draw_data();
				device->EndScene();
			}

			if(device->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
				reset_device();
		}

		int render_wnd_proc(HWND _wnd_handle, UINT msg, WPARAM w_param, LPARAM l_param) override {
			switch(msg) {
				case WM_SIZE: {
					if(device && w_param != SIZE_MINIMIZED) {
						present_parameters.BackBufferWidth = LOWORD(l_param);
						present_parameters.BackBufferHeight = HIWORD(l_param);
						reset_device();
					}
				} return 0;
			}

			return -1;
		}

		void reset_device() {
			invalidate_device_objects();
			if(device->Reset(&present_parameters) == D3DERR_INVALIDCALL) throw std::runtime_error("d3d_device->Reset == D3DERR_INVALIDCALL");
			create_device_objects();
		}
	};
}