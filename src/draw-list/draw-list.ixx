export module null.render:draw_list;
import :font;
import null.sdk;

export namespace null {
	enum struct e_draw_list_flags {
		anti_aliased_lines = 1 << 0,
		anti_aliased_lines_use_texture = 1 << 1,
		anti_aliased_fill = 1 << 2,
		allow_vtx_offset = 1 << 3,
	}; enum_create_bit_operators(e_draw_list_flags, true);
	enum_create_cast_operator(e_draw_list_flags, -);

	enum class e_corner_flags {
		top_left = 1 << 0,
		top_right = 1 << 1,
		bot_left = 1 << 2,
		bot_right = 1 << 3,

		top = top_left | top_right,
		bot = bot_left | bot_right,
		left = top_left | bot_left,
		right = top_right | top_right,

		all = top_left | top_right | bot_left | bot_right
	}; enum_create_bit_operators(e_corner_flags, true);
	enum_create_cast_operator(e_corner_flags, -);

	//flags for drawing text
	enum class e_text_flags : std::uint32_t {
		//the default color for the outline is black, maybe later I will add color settings, but personally I don't need it, so you can always change it in your solution
		//you can change offsets in shared data(text_outline_offsets)
		outline = 1 << 0,

		//default aligin is top and left
		aligin_right = 1 << 1, //will be draw at pos - text_size.x
		aligin_bottom = 1 << 2, //will be draw at pos - text_size.y
		//align_center is calculated after the rest, so it can be combined with left and bottom
		aligin_centre_x = 1 << 3,
		aligin_centre_y = 1 << 4,
		aligin_centre = aligin_centre_x | aligin_centre_y,
		aligin_mask = aligin_centre | aligin_bottom | aligin_right
	}; enum_create_bit_operators(e_text_flags, true);
	enum_create_cast_operator(e_text_flags, -);

	enum class e_cmd_callbacks {
		render_draw_data //std::function<bool(cmd*)>, will call setup_render_state if the callback returns true
	};

	namespace render {
		struct shared_data_t {
			std::vector<vec2_t> text_outline_offsets{ { -1, 0 }, { 0, -1 }, { 0, 1}, { 1, 0 } };
			e_draw_list_flags initialize_flags{ };
			c_font* font{ };
			rect_t clip_rect_fullscreen{ };

			std::array<vec2_t, 12> arc_fast_vtx{ };
			std::array<std::uint8_t, 64> circle_segments{ };
			float circle_segment_max_error{ };
			float curve_tessellation_tol{ };

			shared_data_t(e_draw_list_flags _initialize_flags = e_draw_list_flags{ }) : initialize_flags(_initialize_flags) {
				for(int i = 0; i < arc_fast_vtx.size(); i++) {
					float a = (i * 2.f * std::numbers::pi) / arc_fast_vtx.size();
					arc_fast_vtx[i] = vec2_t{ std::cosf(a), std::sinf(a) };
				}
			}

			void set_circle_segment_max_error(float max_error) {
				if(circle_segment_max_error == max_error) return;
				circle_segment_max_error = max_error;

				for(int i = 0; i < circle_segments.size(); i++) {
					float radius = i + 1.f;
					int segment_count = std::clamp((std::numbers::pi * 2.f) / std::acosf((radius - circle_segment_max_error) / radius), 12., 512.);
					circle_segments[i] = (std::uint8_t)std::min(segment_count, 255);
				}
			}
		} shared_data{ e_draw_list_flags::allow_vtx_offset };

		struct multicolor_text_t {
			using data_t = std::vector<std::pair<std::string, color_t>>;
			data_t data{ };

			//returns a string made up of all the strings in the text
			std::string unite() {
				return std::accumulate(data.begin(), data.end(), std::string{ }, [=](std::string result, data_t::value_type str) {
					return result + str.first;
					});
			}
		};

		class c_draw_list {
		public:
			struct vertex_t {
				vec2_t pos{ }, uv{ };
				color_t color{ };
			};

			struct draw_data_t {
				std::vector<c_draw_list*> layers{ };

				bool valid{ };
				std::vector<c_draw_list*> cmd_lists{ };
				int total_idx_count{ }, total_vtx_count{ };

				vec2_t window_pos{ }, window_size{ };

				void de_index_all_buffers();
				void add_draw_list(c_draw_list* draw_list);
				void setup();

				void scale_clip_rects(vec2_t scale) {
					for(c_draw_list* cmd_list : cmd_lists)
						for(cmd_t& cmd : cmd_list->cmd_buffer) cmd.clip_rect *= scale;
				}
			};

			struct cmd_header_t {
				rect_t clip_rect{ };
				void* texture_id{ };
				std::uint32_t vtx_offset{ };

				class_create_spaceship_operator(cmd_header_t);
			};

			struct cmd_t : cmd_header_t {
				std::uint32_t idx_offset{ }, element_count{ };

				single_callbacks_t<e_cmd_callbacks> callbacks{ };
			};

		public:
			shared_data_t* parent_shared_data{ &shared_data };

			std::vector<void*> texture_id_stack{ };
			std::vector<vec2_t> path{ };
			std::vector<rect_t> clip_rect_stack{ };

			std::vector<cmd_t> cmd_buffer{ };
			cmd_header_t cmd_header{ };

			using idx_buffer_t = std::vector<std::uint16_t>; idx_buffer_t idx_buffer{ };
			using vtx_buffer_t = std::vector<vertex_t>; vtx_buffer_t vtx_buffer{ };

		public:
			void add_draw_cmd() { cmd_buffer.push_back({ cmd_header.clip_rect, cmd_header.texture_id, cmd_header.vtx_offset, (std::uint32_t)idx_buffer.size() }); }
			void pop_unused_draw_cmd() { if(cmd_buffer.empty()) return; if(!cmd_buffer.back().element_count && !cmd_buffer.back().callbacks.empty()) cmd_buffer.pop_back(); }
			
			void reset_for_begin_render() {
				cmd_buffer.clear(); cmd_buffer.push_back(cmd_t{ });
				idx_buffer.clear();
				vtx_buffer.clear();
				clip_rect_stack.clear();
				texture_id_stack.clear();
				path.clear();

				cmd_header = cmd_header_t{ };
			}
			void shade_verts_linear_uv(vtx_buffer_t::iterator vtx_start, vtx_buffer_t::iterator vtx_end, rect_t rect, rect_t uv, bool clamp);
			
			void on_changed_clip_rect();
			void push_clip_rect(rect_t rect, bool intersect_with_current_rect = false);
			void push_clip_rect_fullscreen() { push_clip_rect(parent_shared_data->clip_rect_fullscreen); }
			void pop_clip_rect();
			
			void on_changed_texture_id();
			void push_texture_id(void* texture_id);
			void pop_texture_id();

			void on_changed_vtx_offset();
			void vtx_check(int vtx_count);
			void prim_add_vtx(vertex_t vtx) { vtx_check(1); vtx_buffer.push_back(vtx); }
			void prim_insert_vtx(vtx_buffer_t::iterator place, vtx_buffer_t vtx_list) { vtx_check(vtx_list.size()); vtx_buffer.insert(place, vtx_list.begin(), vtx_list.end()); }
			void prim_insert_vtx(vtx_buffer_t vtx_list) { prim_insert_vtx(vtx_buffer.end(), vtx_list); }
			void prim_write_vtx(vtx_buffer_t::iterator& place, vtx_buffer_t vtx_list) { std::move(vtx_list.begin(), vtx_list.end(), place); place += vtx_list.size(); }
			
			void prim_add_idx(std::uint16_t idx) { idx_buffer.push_back(idx); cmd_buffer.back().element_count++; }
			void prim_insert_idx(idx_buffer_t::iterator place, idx_buffer_t idx_list) { idx_buffer.insert(place, idx_list.begin(), idx_list.end()); cmd_buffer.back().element_count += idx_list.size(); }
			void prim_insert_idx(idx_buffer_t idx_list) { prim_insert_idx(idx_buffer.end(), idx_list); }
			void prim_write_idx(idx_buffer_t::iterator& place, idx_buffer_t idx_list) { std::move(idx_list.begin(), idx_list.end(), place); place += idx_list.size(); cmd_buffer.back().element_count += idx_list.size(); }
			
			void prim_rect(vec2_t a, vec2_t c, color_t color);
			void prim_rect_uv(vec2_t a, vec2_t c, vec2_t uv_a, vec2_t uv_c, color_t color);
			void prim_quad_uv(std::array<vec2_t, 4> points, std::array<vec2_t, 4> uvs, color_t color);

			void path_rect(vec2_t a, vec2_t b, float rounding = 0.0f, e_corner_flags flags = e_corner_flags::all);
			void path_arc_to_fast(vec2_t center, float radius, int a_min_of_12, int a_max_of_12);
			void path_fill_convex(color_t clr) { draw_convex_poly_filled(path, clr); path.clear(); }
			void path_stroke(color_t color, bool closed, float thickness) { draw_poly_line(path, color, closed, thickness); path.clear(); }

			void draw_text(std::string str, vec2_t pos, color_t color, e_text_flags flags = e_text_flags{ }, c_font* font = nullptr, float size = 0.f) { draw_text(multicolor_text_t{ { { str , color } } }, pos, flags, font, size); }
			void draw_text(multicolor_text_t str, vec2_t pos, e_text_flags flags = e_text_flags{ }, c_font* font = nullptr, float size = 0.f);
			void draw_rect(vec2_t a, vec2_t b, color_t color, float thickness = 1.f, float rounding = 0.f, e_corner_flags flags = e_corner_flags::all);
			void draw_rect_filled(vec2_t a, vec2_t b, color_t color, float rounding = 0.f, e_corner_flags flags = e_corner_flags::all);
			void draw_convex_poly_filled(std::vector<vec2_t> points, color_t color);
			void draw_poly_line(std::vector<vec2_t> points, color_t color, bool closed, float thickness = 1.f);
		};

		c_draw_list::draw_data_t draw_data{ };
		c_draw_list background_draw_list{ }, foreground_draw_list{ };
	}
}