#pragma once

#include "dcon_generated.hpp"
#include "gui_element_types.hpp"
#include "gui_graphics.hpp"
#include "province.hpp"
#include "text.hpp"
#include "unit_tooltip.hpp"

namespace ui {

struct update_position { };

class unit_icon_color : public image_element_base {
	int32_t get_frame(sys::state& state) noexcept {
		if(parent) {
			Cyto::Any payload = dcon::nation_id{};
			parent->impl_get(state, payload);
			dcon::nation_id content = any_cast<dcon::nation_id>(payload);

			if(content == state.local_player_nation)
				return 0; // green, ourselves
			auto drid = state.world.get_diplomatic_relation_by_diplomatic_pair(state.local_player_nation, content);
			if(state.world.diplomatic_relation_get_are_allied(drid))
				return 0; // green, allies, puppets and us
			else {
				auto iid = state.world.nation_get_identity_from_identity_holder(content);
				for(auto wa : state.world.nation_get_war_participant(state.local_player_nation))
					for(auto o : wa.get_war().get_war_participant())
						if(o.get_is_attacker() != wa.get_is_attacker() && o.get_nation().get_identity_from_identity_holder().id == iid)
							return 1; // red, enemy
			}
		}
		return 2; // gray, i.e not involved or irrelevant for us
	}

public:
	void on_update(sys::state& state) noexcept override {
		frame = get_frame(state);
	}
};

class unit_strength_text : public simple_text_element_base {
public:
	void on_update(sys::state& state) noexcept override {
		if(parent) {
			Cyto::Any payload = float{0.f};
			parent->impl_get(state, payload);
			float content = any_cast<float>(payload);
			set_text(state, text::prettify(int32_t(content)));
		}
	}
};

class unit_icon_button : public button_element_base {
public:
	void button_action(sys::state& state) noexcept override {
		if(parent) {
			Cyto::Any payload = dcon::province_id{};
			parent->impl_get(state, payload);
			dcon::province_id content = any_cast<dcon::province_id>(payload);
			state.world.province_for_each_army_location_as_location(content, [&](dcon::army_location_id id) {
				// Force the Reorg window to be cleaned up
				Cyto::Any cpayload = element_selection_wrapper<unitpanel_action>{unitpanel_action{unitpanel_action::close}};
				state.ui_state.army_status_window->impl_get(state, cpayload);
				state.ui_state.navy_status_window->impl_get(state, cpayload);

				state.ui_state.army_status_window->set_visible(state, true);
				state.ui_state.navy_status_window->set_visible(state, false);

				auto aid = state.world.army_location_get_army(id);
				Cyto::Any d_payload = element_selection_wrapper<dcon::army_id>{aid};
				state.ui_state.army_status_window->impl_get(state, d_payload);
			});
			state.world.province_for_each_navy_location_as_location(content, [&](dcon::navy_location_id id) {
				// Force the Reorg window to be cleaned up
				Cyto::Any cpayload = element_selection_wrapper<unitpanel_action>{unitpanel_action{unitpanel_action::close}};
				state.ui_state.army_status_window->impl_get(state, cpayload);
				state.ui_state.navy_status_window->impl_get(state, cpayload);

				state.ui_state.army_status_window->set_visible(state, false);
				state.ui_state.navy_status_window->set_visible(state, true);

				state.ui_state.army_combat_window->set_visible(state, false);
				state.ui_state.naval_combat_window->set_visible(state, true);

				auto nid = state.world.navy_location_get_navy(id);
				Cyto::Any d_payload = element_selection_wrapper<dcon::navy_id>{nid};
				state.ui_state.navy_status_window->impl_get(state, d_payload);
				auto fat = dcon::fatten(state.world, any_cast<element_selection_wrapper<dcon::navy_id>>(d_payload).data);
				if(!fat.get_battle_from_navy_battle_participation().is_valid()) {
					state.ui_state.naval_combat_window->set_visible(state, false);
				} else {
					state.ui_state.naval_combat_window->impl_get(state, d_payload);
					state.ui_state.naval_combat_window->impl_on_update(state);
				}
			});
		}
	}

	void on_update(sys::state& state) noexcept override {
		if(parent) {
			Cyto::Any payload = dcon::province_id{};
			parent->impl_get(state, payload);
			dcon::province_id content = any_cast<dcon::province_id>(payload);
			// TODO - hacky, better solution probably exists
			state.world.province_for_each_army_location_as_location(content, [&](dcon::army_location_id id) {
				if(dcon::fatten(state.world, id).get_army().get_army_control().get_controller().id == state.local_player_nation) {
					frame = 0;
				} else {
					frame = 2;
				}
			});
			state.world.province_for_each_navy_location_as_location(content, [&](dcon::navy_location_id id) {
				frame = 1;
			});
		}
	}


	tooltip_behavior has_tooltip(sys::state& state) noexcept override {
		return tooltip_behavior::variable_tooltip;
	}

	void update_tooltip(sys::state& state, int32_t x, int32_t y, text::columnar_layout& contents) noexcept override {
		populate_unit_tooltip(state, contents, retrieve<dcon::province_id>(state, parent));
	}
};

class unit_icon_window : public window_element_base {
	unit_strength_text* strength_text = nullptr;
	image_element_base* attr_icon = nullptr;
public:
	bool visible = true;
	bool province_is_populated = false;

	dcon::province_id content{};

	std::unique_ptr<element_base> make_child(sys::state& state, std::string_view name, dcon::gui_def_id id) noexcept override {
		if(name == "unit_panel_bg") {
			return make_element_by_type<unit_icon_button>(state, id);
		} else if(name == "unit_panel_country_flag") {
			auto ptr = make_element_by_type<flag_button>(state, id);
			ptr->base_data.position.y -= 1; // Nudge
			return ptr;
		} else if(name == "unit_panel_color") {
			return make_element_by_type<unit_icon_color>(state, id);
		} else if(name == "unit_strength") {
			auto ptr = make_element_by_type<unit_strength_text>(state, id);
			ptr->base_data.position.y -= 1; // Nudge
			strength_text = ptr.get();
			return ptr;
		} else if(name == "unit_panel_org_bar") {
			return make_element_by_type<vertical_progress_bar>(state, id);
		} else if(name == "unit_panel_attr") {
			auto ptr = make_element_by_type<image_element_base>(state, id);
			attr_icon = ptr.get();
			return ptr;
		} else
			return nullptr;
	}

	void on_update(sys::state& state) noexcept override {
	 province_is_populated = false;
		state.world.province_for_each_army_location_as_location(content, [&](dcon::army_location_id id) { province_is_populated = true; });
		state.world.province_for_each_navy_location_as_location(content, [&](dcon::navy_location_id id) { province_is_populated = true; });

		bool has_attrition = false; // TODO: Attrition
		attr_icon->set_visible(state, has_attrition);
	}

	void impl_render(sys::state& state, int32_t x, int32_t y) noexcept override {
		if(!province_is_populated)
			return;

		auto mid_point = state.world.province_get_mid_point(content);
		auto map_pos = state.map_state.normalize_map_coord(mid_point);
		auto screen_size =
				glm::vec2{float(state.x_size / state.user_settings.ui_scale), float(state.y_size / state.user_settings.ui_scale)};
		glm::vec2 screen_pos;
		if(!state.map_state.map_to_screen(state, map_pos, screen_size, screen_pos)) {
			visible = false;
			return;
		}
		visible = true;
		auto new_position = xy_pair{int16_t(screen_pos.x - 25), int16_t(screen_pos.y - 20)};
		window_element_base::base_data.position = new_position;
		window_element_base::impl_render(state, new_position.x, new_position.y);
	}

	message_result get(sys::state& state, Cyto::Any& payload) noexcept override {
		if(payload.holds_type<dcon::province_id>()) {
			payload.emplace<dcon::province_id>(content);
			return message_result::consumed;
		} else if(payload.holds_type<dcon::nation_id>()) {
			dcon::nation_id nation_id{};
			state.world.province_for_each_army_location_as_location(content, [&](dcon::army_location_id id) {
				auto aid = state.world.army_location_get_army(id);
				nation_id = state.world.army_control_get_controller(state.world.army_get_army_control_as_army(aid));
			});
			state.world.province_for_each_navy_location_as_location(content, [&](dcon::navy_location_id id) {
				auto nid = state.world.navy_location_get_navy(id);
				nation_id = state.world.navy_control_get_controller(state.world.navy_get_navy_control_as_navy(nid));
			});
			payload.emplace<dcon::nation_id>(nation_id);
			return message_result::consumed;
		} else if(payload.holds_type<float>()) {
			float strength = 0.f;
			state.world.province_for_each_army_location_as_location(content, [&](dcon::army_location_id id) {
				auto aid = state.world.army_location_get_army(id);
				state.world.army_for_each_army_membership_as_army(aid, [&](dcon::army_membership_id amid) {
					auto rid = state.world.army_membership_get_regiment(amid);
					strength += state.world.regiment_get_strength(rid);
				});
			});
			state.world.province_for_each_navy_location_as_location(content, [&](dcon::navy_location_id id) {
				auto nid = state.world.navy_location_get_navy(id);
				state.world.navy_for_each_navy_membership_as_navy(nid, [&](dcon::navy_membership_id nmid) {
					auto sid = state.world.navy_membership_get_ship(nmid);
					strength += 1.f;
				});
			});
			payload.emplace<float>(strength);
			return message_result::consumed;
		}
		return message_result::unseen;
	}

	mouse_probe impl_probe_mouse(sys::state& state, int32_t x, int32_t y,
			mouse_probe_type type) noexcept override {
		if(visible && province_is_populated)
			return window_element_base::impl_probe_mouse(state, x, y, type);
		else
			return mouse_probe{ nullptr, ui::xy_pair{} };
	}

};

class rgo_icon : public image_element_base {
public:
	dcon::province_id content{};
	void impl_render(sys::state& state, int32_t x, int32_t y) noexcept override {
		auto mid_point = state.world.province_get_mid_point(content);
		auto map_pos = state.map_state.normalize_map_coord(mid_point);
		auto screen_size =
				glm::vec2{float(state.x_size / state.user_settings.ui_scale), float(state.y_size / state.user_settings.ui_scale)};
		glm::vec2 screen_pos;
		if(!state.map_state.map_to_screen(state, map_pos, screen_size, screen_pos))
			return;
		auto new_position = xy_pair{int16_t(screen_pos.x - base_data.size.x / 2), int16_t(screen_pos.y - base_data.size.y / 2)};
		image_element_base::base_data.position = new_position;
		image_element_base::impl_render(state, new_position.x, new_position.y);
	}
	void on_update(sys::state& state) noexcept override {
		auto cid = state.world.province_get_rgo(content).id;
		frame = int32_t(state.world.commodity_get_icon(cid));
	}
};
} // namespace ui
